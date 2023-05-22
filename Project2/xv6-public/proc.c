#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "thread.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

uint iomutex = 0; //* 0 - not using, 1 - using

thread_t threadlist[NPROC] = {0};

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

//*debug: find where panic('acquire') or panic('release') happens.
void
renamelock(char* name){
  ptable.lock.name = name;
}

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
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;
  //thread_t *th = &init_thread; //*Init Thread

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;
  //sp = p->kstack + THREADSPACE;
  //p->tstack = sp;
  //sp = sp + (KSTACKSIZE - THREADSPACE);
  //* Current Stack Bottom: Thread space bottom
  //p->tstack = sp;

  //p->tstack = sp; //* Thread Stack bottom.
  /*
  // * Allocate 100 thread space in here.
  // * Each Thread: requires 20 bytes.
  for(int i = 0; i < TMAXPERPROC; i++){
    memset(sp, 0, sizeof(thread_t));
    p->threads[i] = (struct thread_t*)sp; // * Allocate address
    sp -= sizeof(thread_t);
  }
  p->tstack = sp; // * Thread Stack Top. */

  /*
   *  --------
   * |  2048  |
   * |  proc  |
   * |        |
   *  --------
   * |  2048  |
   * | thread |
   * |        |
   *  --------
  * */
  
  //cprintf("[%d], Thread Stack: %d / Process stack: %d\n",p->pid,  p->tstack, p->kstack);

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

  //* Allocated stacksize and memory limit (Default)
  p->stacksize = 1;
  p->memlim = 0;
  p->isthread = 0;
  //* Thread init.
  //p->thread = 0;
  p->thctr = 0;
  //memset(p->thread, 0, sizeof(thread_t *));
 
  //* Allocate initialized thread to process.
  p->threadnum = 0; //* Empty thread.

  //cprintf("[Process %d], esp: %d\n", p->pid, p->tf->esp);

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

  //* Check memory limit
  if(curproc->memlim != 0 && sz + n > curproc->memlim) { //* Memory limit exists, but current process grows more than its limitation.
    cprintf("FATAL ERROR: Out of memory - allocated more than its limitation.\n");
    return -1;
  }


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
  if(curproc->isthread == 1){
    np->parent = curproc->thread->parent; //* Prevent process being zombie if thread terminated.
  }
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
  //struct proc* thr;
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

  //* parent exit: check all thread is done, if not, kill them.
  /*for(thr = ptable.proc; thr < &ptable.proc[NPROC]; thr++){
    if(thr->isthread == 1){
      if(thr->thread->parent == curproc && thr->thread->exitcalled == 0){
        thr->killed = 1;
      }
    }
  }*/
  if(curproc->threadnum > 0){
    purgethreads(curproc, 0);
  }

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
	pid = p->pid;
	if(p->isthread == 1){
	  //* go to cleanupthread routine.
	  cleanupthread(p);
	}else{
          kfree(p->kstack);
          p->kstack = 0;
          freevm(p->pgdir);
          p->pid = 0;
          p->parent = 0;
          p->name[0] = 0;
          p->killed = 0;
	  //* Thread reset
	  p->isthread = 0;
	  p->thread = 0;
          p->state = UNUSED;
	}
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
void
scheduler(void)
{
  struct proc *p;
  //thread_t *cthread;
  struct cpu *c = mycpu();
  c->proc = 0;
  for(;;){
    // Enable interrupts on this processor.
    sti();
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;
       
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      //cprintf("Context: eip - %d\n", p->context->eip);
      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
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

  //* Determine switching: thread or process
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

//* Project #2
//
//* list: list all current process running
int
list(){
  struct proc *p;
  struct proc *t;
  uint procsz;

  cprintf("-----------------------------------------------------------------------------------------------------------------\n");
  cprintf("| [PID] name / number of stack page / allocated memory (byte) / memory limit (byte) / Number of running threads |\n");
  cprintf("-----------------------------------------------------------------------------------------------------------------\n");
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    procsz = 0; //* Init
    if(p->state == UNUSED || p->isthread == 1 || p->state == ZOMBIE) //* skip the unused space and thread.
      continue;

    procsz = p->sz;
    if(p->threadnum > 0){
      //* Find threads and sum it all.
      for(t = ptable.proc; t < &ptable.proc[NPROC]; t++){
        if(t->isthread == 1){
	  if(t->thread->parent == p){
	    procsz += t->sz; //* Add thread process size
	  }
	}
      }
    }

    cprintf("[%d] / %s / %d stacks / %d bytes allocated /" , p->pid, p->name, p->stacksize, procsz);
    p->memlim == 0 ? cprintf(" UNLIMITED /") : cprintf(" %d byte(s) /", p->memlim); //* memlim will be 0 if there is no memeory limitation.
    cprintf(" Running %d threads\n", p->threadnum);

  }

  return 0;
}

//* setmemorylimit: set memory limit for process.
int
setmemorylimit(int pid, int limit){
 struct proc *p;

 for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
   if(p->pid != pid)
     continue;

   //* Process Found
   break;
 }

 if(limit != 0 && limit < p->sz) //* Requested limit is less than size. (0: no limitation)
   return -1; 

 //* Limit Allocation
 p->memlim = limit; 
 //* This limitation will be enforced in growproc();

 return 0;
}

//* allocthread(thread_t *thread)
//* Referred: thread_create()
//* allocate memory space to thread.
//* Implemented based on code of fork() and exec().
int
allocthread(thread_t *thread){
  struct proc* curproc = myproc();
  struct thread_t* destthread;
  uint sp = 0; 
  uint ustack[2]; //* size: basic stack 2:
		  // fake return counter, address of argument, termination
  int tid = ++(curproc->thctr); //* Thread counter will be new thread id.
  struct proc* newthread;
  pde_t *pgdir = 0;

  //* Step 1) Thread init
  //* Find Vacant Thread.
  
  for(destthread = threadlist; destthread < &threadlist[NPROC]; destthread++){
    if(destthread->occupied == 0){
      break;
    }
  }
  

  if(destthread >= &threadlist[NPROC]){
    //*Cannot find space.
    cprintf("Allocation Failed while finding thread space.\n");
    goto failed;
  }

  //* Allocation
  //
  thread->pid = curproc -> pid;
  thread->tid = tid;
  thread->parent = curproc;
  thread->retval = 0;
  thread->exitcalled = 0;

  destthread->pid = curproc->pid;
  destthread->tid = tid;
  destthread->parent = curproc;
  destthread->retval = 0;
  destthread->exitcalled = 0;
  destthread->start_routine = thread->start_routine;
  destthread->arg = thread->arg;
  //*Now Occupied
  destthread->occupied = 1;

  //*Start Allocation.
  if((newthread = allocproc()) <= 0){
    cprintf("Allocation Failed while making thread's space\n");
    goto failed;
  } //* thread found.

  //* 2) New Thread init (process -> thread)
  //* Thread will be made based on parent's info.
  //* Allocate thread
  newthread->pid = curproc->pid;
  newthread->isthread = 1; // * This process is thread.
  newthread->sz = curproc->sz; //* Thread size will follow parent's size.
  //* Copy parent's trap frame
  *newthread->tf = *curproc->tf;

  //* 2) Thread Allocation.
  newthread->thread = destthread;
  
  //* 3)Executable Stack Allocation.
  //* Copy parent's page directory. - Referrences fork();
  if((pgdir = copyuvm(curproc->pgdir, curproc->sz)) <= 0){
    cprintf("Allocation Failed while copying parent's page\n");
    goto failed;
  }
  //pgdir = shareuvm(pgdir, curproc);


  //* Allocate stack frame.
  ustack[0] = (uint)0xffffffff;
  ustack[1] = (uint)newthread->thread->arg;
  //* clone stack

  sp = newthread->sz;
  sp -= (uint)(sizeof(ustack));

  //void* stack_mem = kalloc();
  //memmove(stack_mem, ustack, sizeof(ustack) + 1);

  if(copyout(pgdir, sp, ustack ,(sizeof(ustack))) < 0){
    cprintf("Allocation failed while copying argument stacks\n");
    goto failed;
  }

  //* 4) Entry Point Setting
  newthread->tf->eax = 0;
  newthread->tf->eip = (uint)newthread->thread->start_routine;
  newthread->tf->esp = sp;

  //* 5) file copy - reffered fork()
  //* I think this was unnecessary
  //* however if I remove this, I get wierd deadlock :(
  for(int i = 0; i < NOFILE; i++){
    if(curproc->ofile[i]){
      newthread->ofile[i] = filedup(curproc->ofile[i]);
    }
  }
  newthread->cwd = idup(curproc->cwd);

  //* 6) connect pgdir and miscellaneous things.
  //* copy name
  safestrcpy(newthread->name, curproc->name, sizeof(curproc->name));
  //* give completed pgdir.
  newthread->pgdir = pgdir;
  //* Make it runnable state.
  acquire(&ptable.lock);
  newthread->state = RUNNABLE;
  //newthread->thread = thread;
  release(&ptable.lock);

  //newthread->thread = thread;
  ++(newthread->threadnum);
  return 0;


failed:
  if(pgdir){ // * Free allocated paged bc failed.
    freevm(pgdir);
  }

  cprintf("thread allocation failed\n");
  return -1;
}

//* terminatethread()
//* Referred: thread_exit()
//* made based on exit()
void
terminatethread(void* retval){
  struct proc* curthread = myproc(); //* This process must be thread.
  //int fd;

  acquire(&ptable.lock);
  //* Step 1) set return value.
  curthread->thread->retval = retval;
  curthread->thread->exitcalled = 1; //* This thread had just been ended.
  //* Not cloing opened file in here
  //* Removing it triggers panic('acquire') at before swtch() call in scheduler.

  //* Step 3) Make this state as zombie.
  //* Not disallocate this thread right now; need to collect retval in thread_join();
  curthread->state = ZOMBIE; //* Zombie State
  
  //* Step 4) Wake up parent process, and call sched();
  wakeup1(curthread->thread->parent);
  sched();
  //release(&ptable.lock);
}

//* waitthread(thread_t thread, void** retval)
//* Referred: thread_join
//* wait until thread ends.
//* implemented based on wait();
//* Funtion called thread_join will perform circular wait until thread ends.
int
waitthread(thread_t thread, void** retval){
  //* Step 1) Find targeting thread.
  struct proc* tgtthread = 0;
  struct proc* curproc = myproc();

  acquire(&ptable.lock);

  for(tgtthread = ptable.proc; tgtthread < &ptable.proc[NPROC]; tgtthread++){
    if((tgtthread->isthread == 1) && (tgtthread->thread->tid == thread.tid))
      break; //* Thread Found
  }

 
  if(tgtthread == 0){
    cprintf("thread_join: invalid thread.\n");
    release(&ptable.lock);
    return -1;
  }

  if(tgtthread->thread->parent != curproc){
    // * Thread can only be joined to it's caller.
    cprintf("thread_join: not a caller\n");
    release(&ptable.lock);
    return -1;
  }

  // * Step 2) wait until current thread end.
  while(1){ // * Circular Wait.
    if((tgtthread->thread->exitcalled == 1) && (tgtthread->state == ZOMBIE)){
      // * Current thread had just been ended.
      // * Step 3) Save Return Value.
      *retval = tgtthread->thread->retval;
      // * Step 4) Clean up thread.
      cleanupthread(tgtthread);
      release(&ptable.lock);
      return 0;
    }

    // * Sleep until it's child calls thread_exit(), waking up its parent(current process).
    sleep(curproc, &ptable.lock);
  }

  release(&ptable.lock);
  return -1;
}

//* cleanupthread(sturct proc* tgtthread)
//* Referred:  waitthread(), thread_join().
//* deallocate, and clean thread space
//* CAUTION: MUST CALL WITH LOCKED PTABLE (acquire(&ptable.lock))
void
cleanupthread(struct proc* tgtthread){
  //* Find parent
  struct proc* parent = tgtthread->thread->parent;
  //* Start Cleaning Procedure.
  //* Clean thread space
  tgtthread->thread->pid = 0;
  tgtthread->thread->tid = 0;
  tgtthread->thread->exitcalled = 0;
  tgtthread->thread->retval = 0;
  tgtthread->thread->start_routine = 0;
  tgtthread->thread->parent = 0;
  tgtthread->thread->arg = 0;
  tgtthread->thread->occupied = 0;
  tgtthread->thread = 0;


  //* Clean thread process.
  kfree(tgtthread->kstack);
  tgtthread->kstack = 0;
  freevm(tgtthread->pgdir);
  tgtthread->pid = 0;
  tgtthread->parent = 0;
  tgtthread->name[0] = 0;
  tgtthread->killed = 0;
  tgtthread->state = UNUSED;

  //* Reduce number of thread.
  --(parent->threadnum);
}

//*purgethreads()
//* Kill all thread it have.
void
purgethreads(struct proc* p, struct proc* exception){
  struct proc* tgt;

  for(tgt = ptable.proc; tgt < &ptable.proc[NPROC]; tgt++){
    if(tgt->isthread == 1){
      if(tgt != exception && tgt->thread->parent == p){
        //* If current thread is not the exception (thread must be alive) 
	//and current thread's parent is the same process passed from argument.
	cleanupthread(tgt);
      }
    }
  }
}

//* iomutex
void
iomutexcheckin(void){
  //* If some other process using io resources,
  //* wait until it releases.
  while(iomutex){
    yield();  
  }

  iomutex = 1; //* current process will use io resource
}

void
iomutexcheckout(void){
  iomutex = 0;
}



