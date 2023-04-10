#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h" 
#include "spinlock.h"

#define MLFQ_LEV 3 //* Define MLFQ Total Level: Used to indicate number of process in each level.

//* Ptable = Same as before, all process are managed in current ptable.
struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

struct proc *L[MLFQ_LEV][NPROC] = {0}; //* Initialize as 0 (NULL) 
//* L[0] => L0: Most prioritized process queue, RR, TQ: 4 ticks
//* L[1] => L1: process queue, RR, TQ: 6 ticks
//* L[2] => L2: process queue, Priority Scheduling based on proc()->priority, FCFS for same priority, TQ: 8 tick;

uint arrived = 0; //* arrived - assign current value to process demoted to L2. The value will be increased proportional to number of process demoted to L2.

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.

//* Allocate new process - New Process goes to L0 (The highest level of the queue)
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) //* Search for unused space in L0
    if(p->state == UNUSED)
      goto found; 

  release(&ptable.lock);
  return 0;

 found:
  int idx = 0; //* Used to search unused space at queue L0.

  p->state = EMBRYO;
  p->pid = nextpid++;
  p->level = 0; //* Every new process are allocated at level 0.
  p->priority = 3; //* Default priority will be 3.
  p->arrived = 0; //* Default arrived value will be 0.

 for(idx = 0; idx < NPROC; idx++){
   if(L[0][idx] == 0) { //* If there is no assiged process for current index,
     L[0][idx] = p; //* Assign current process to index - Scheduled in L0 initially.
     p->idx = idx; //* Assign Index - current position
     cprintf("Allocated Process: %s // PID: %d, Allocated in L0[%d]\n", p->name, p->pid, idx); //* Debug: Comment this line if it is not required.
     break;
   }
 }

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
	// Found one.

	//* Remove current process from the queue.
        int level = p->level;
        int idx = p->idx;
        L[level][idx] = 0;
        //* Now, current cell is usuable for another new process.
        
	cprintf("RELEASED PROCESS -> PID: %d / LEVEL: %d / INDEX: %d / PRIORITY: %d\n", p->pid, p->level, p->idx, p->priority);

	pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
	//* Reset property added for MLFQ.
	p->level = 0;
	p->idx = 0;
	p->priority = 0;
	p->arrived = 0;

        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.


//* ELE3021 - Project #1: Make MLFQ Scheduler

//retlevel: return level of queue has RUNNABLE process
int
retlevel(void)
{ //* Return level of queue for next process
  //* Searches for the Runnable, returns level where the RUNNABLE process firstly found.

  int level = 0;
  int idx = 0;

  for(level = 0; level < MLFQ_LEV; level++)
  {
    if(level == 2){
      //* If current level is 2, it means there is no RUNNABLE process for 0 and 1. It is reasonable for early return of level.
      goto RET;
    }
    for(idx = 0; idx < NPROC; idx++)
    {
      if(L[level][idx] != 0 && L[level][idx]->state == RUNNABLE)
      {
        goto RET; //* If RUNNABLE process found, return current level.
      }
    }
  }

RET:
  return level;
}

//*demoteproc - demote process level to lower level.
int
demoteproc(void)
{
  if(myproc()->level < 2) //* level 0 -> level 1, level 1 -> level 2
  {
    int idx = 0; //* new index for demoted process.
    int new_level = myproc()->level + 1;
    
    for(idx = 0; idx < NPROC; idx++)
    {
      if(L[new_level][idx] == 0) //* Empty cell found
      {
	L[myproc()->level][myproc()->idx] = 0; //* remove current process from previous level.
	L[new_level][idx] = myproc(); //* Assign current process to new level.
	myproc()->idx = idx; //* Gives new index for current queue.
	myproc()->level = new_level; //* Update process level.
	
	if(new_level == 2) //* If new level is L2, gives new arrived value.
	  myproc()->arrived = arrived++;

	cprintf("Demoted Process: %s // PID: %d, Allocated in L%d[%d]\n", myproc()->name, myproc()->pid , myproc()->level, idx); //* Debug: Comment this line if it is not required.
      	break; //* Exit Loop
      }

    }
  }
  else
  {
    panic("demoteproc()");
  }

  return 0;
}

//* incpriority(): increase current priority level.
void
incpriority(void)
{
  if(myproc()-> priority > 0)
    (myproc()->priority)--; //* Increase current priority;

  cprintf("Increased Priority // PID: %d, Priority became %d\n", myproc()->pid, myproc()->priority); //* Debug: Comment this line if it is not required.

  return;
}

void
scheduler(void)
{
  int lidx[2] = {0, 0}; //* Index of each Queue - index for L0, L1 needs to be memorized.
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;

  for(;;){
    int level = retlevel(); //* Level of queue - Initialize, determine the level of queue after the loop is over
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    
    //* MLFQ Rule:
        // L0: RR, Mostly Prioritized.
        // L1: RR
        // L2: Priority Scheduling based on process->priority, FCFS for the same priority level.
    
    if(level < 2) //* L0, L1 - RR
    {
      int *idx = &lidx[level]; // * Index for Queue.
      *idx = (*idx) % NPROC; //* Prevents overflow, also allows to restart the same queue for new process

      for(; *idx < NPROC; (*idx)++)
      {
        //* Searches for level of queue needed to be executed.

        int new_level = retlevel();
	//*Check if level is different; if new level is smaller(priortized) than current level, the index must be reset to 0.
	if(new_level == 2) //* L2 - needs different logic - jumps to L2 logic
  	  goto L2;

        if(new_level < level)
        {
          lidx[new_level] = 0; //*Reset to 0.
        }
        if(new_level != level)
        {
          level = new_level; //* changes to new level
	  if(lidx[level] >= NPROC) //* After switch, assume new queue reached the end of the queue, and needs to be reset.
	    lidx[level] = lidx[level] % NPROC;
          idx = &lidx[level];
        }
        //cprintf("Level: %d, Idx: %d\n", level, *idx);
        if(L[level][*idx] == 0)
          continue; // * empty cell - moves to next cell
	  
        if(L[level][*idx]->state != RUNNABLE)
          continue; // * Not runnable process - moves to next cell
	  
        p = L[level][*idx]; // *Assign current process.
        c->proc = p;
        switchuvm(p);
        p->state = RUNNING;

        //cprintf("[PID: %d] AT RUNNING STATE\n", p->pid);

        swtch(&(c->scheduler), p->context);
        switchkvm();

        //cprintf("[PID: %d] SWITCHED CONTEXT\n", p->pid);
        c->proc = 0;
      }
    //release(&ptable.lock);
    }
    else if(level == 2) // * L2 - Priority Queue based on process priority, FCFS for the same priority (Only executed if there are no runnable process in L0 and L1.)
    {
L2:
      int tgt_idx = -1; // * target process index;
      int priority = 1000; // * priority; initialized by the LOWEST value that can't be assigned to process.
      int arrived = -1; // * arrived: initialized by -1. It is initialized there is no cap for arrived; it needs to be initialized by the first process.
      int idx = 0; // * no need to memorize index for L2 -> search the whole L2 queue to find process every time.

      for(idx = 0; idx < NPROC; idx++)
      { // * Find the highest priority with highest arrived value. 
        if(L[2][idx] == 0)
          continue; // * empty cell - moves to next cell
        if(L[2][idx]->state != RUNNABLE) 
	  continue; // * Not runnable process - moves to next cell
 	if(L[2][idx]->priority > priority || (L[2][idx]->arrived > arrived && arrived != -1)) // if arrived haven't be initialized, than arrived will not be included in condition.
	  continue; // * Lower Priority or comes late - moves to next cell
	
	// * FOUND IT
	tgt_idx = idx;
	priority = L[2][idx]->priority; // * Update Value - It must be higher than this priority
	arrived = L[2][idx]->arrived; // * Update Value - It must arrive faster than this arrival time.
      }
      // * Execute the found process
      if(tgt_idx != -1 && priority != 1000)
      { // * Process Found: tgt_idx, priority must be updated for the targeting process
	
	p = L[2][tgt_idx]; // *Assign current process.
        c->proc = p;
        switchuvm(p);
        p->state = RUNNING;

        //cprintf("[PID: %d] AT RUNNING STATE\n", p->pid);

        swtch(&(c->scheduler), p->context);
        switchkvm();

        //cprintf("[PID: %d] SWITCHED CONTEXT\n", p->pid);
        c->proc = 0;
      }
    }
    else //* ERROR
    {
      panic("MLFQ scheduler()");
    }
    release(&ptable.lock);
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

//* Create Wrapper Function for yield()
int sys_yield (void)
{
  cprintf("yield() called in proc.c\n");
  yield();
  return 0;
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

//* Print all process in ptable. (Prints all process available and its state)
void 
printproc(void)
{
  struct proc *p;

  cprintf("====================CURRENT PROCESS AVAILABLE====================\n");
  cprintf("[PID] level / index / priority / state \n");
  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if(p->state != UNUSED)
    {
      cprintf("[%d] %d / %d / %d\n", p->pid, p->level, p->idx, p->priority);
    }
  }
}

int sys_printproc(void)
{
  printproc();
  exit();
  return 0;
}

//* Print all process based on MLFQ. (Prints all process in MLFQ based on its level.)
void 
printmlfq(void)
{
  struct proc *p;
  int level = 0;
  int idx = 0;
  cprintf("====================CURRENT MLFQ STATUS=====================\n");
  cprintf("[PID] level / index / priority / (arrived: for L2)\n");

  for(level = 0; level < 3; level++)
  {
    cprintf("*********************L%d ", level);
    if(level == 0)
      cprintf("- RR, Mostly Prioritized. ********************\n");
    else if(level == 1)
      cprintf("- RR, Took a backseat to L0. ********************\n");
    else if(level == 2)
      cprintf("- Priority Queue. FCFS for same priority ********************\n");

    for(idx = 0; idx < NPROC; idx++)
    {
      if(L[level][idx] == 0)
        continue;

      p = L[level][idx];
      if(level != 2)
        cprintf("[%d] %d / %d / %d\n", p->pid, p->level, p->idx, p->priority);
      else
	cprintf("[%d] %d / %d / %d / %d\n", p->pid, p->level, p->idx, p->priority, p->arrived);
    }
  }
}

int sys_printmlfq(void)
{
  printmlfq();
  exit();
  return 0;
}
