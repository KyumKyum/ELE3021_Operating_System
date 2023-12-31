#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks; //* This Variable will work as a global tick.

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);
  SETGATE(idt[128], 1, SEG_KCODE<<3, vectors[128], DPL_USER); //Practice: enable another syscall entrypoint $128
  SETGATE(idt[129], 1, SEG_KCODE<<3, vectors[129], DPL_USER); //Interrupt Implement: Interrupt 129 calls schedulerLock()
  SETGATE(idt[130], 1, SEG_KCODE<<3, vectors[130], DPL_USER); //Interrupt Implement: Interrupt 130 calls schedulerUnlock() 
  SETGATE(idt[T_RSTGT], 1, SEG_KCODE<<3, vectors[131], DPL_USER); //* Reset Global Tick: called if scheduler Lock called.

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  if(tf->trapno == T_RSTGT)
  { //* Reset global tick
    if(myproc()->killed)
      exit();

    acquire(&tickslock);
    ticks = 0; //* Reset Tick;
    wakeup(&ticks);
    release(&tickslock);

    if(myproc()->killed)
      exit();
    return;
  }

  if(tf->trapno == 128) { //Practice: new entrypoint for syscall
    if(myproc()->killed) {
      exit();
    }
    cprintf("User Interrupt %d called! \n", tf->trapno);
    if(myproc()->killed){
      exit();
    }

    return;
  }

  if(tf->trapno == 129) { //scheduleLock()
    if(myproc()->killed){ //If current process is already killed, there is no reason to make this interrupt happen.
      exit();
    }
    schedulerLock(2019014266);
    if(myproc()->killed){ //After executing system call, if current process killed, exit.
      exit();
    }

    return;
  }

  if(tf->trapno == 130) { //scheduleUnlock()
    if(myproc()->killed){
      exit();
    }
    schedulerUnlock(2019014266);
    if(myproc()->killed){
      exit();
    }

    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      //* Apply Priority Boosting based on global ticks.
      if(ticks % 100 == 0) //* For each 100 global ticks,
      { 
	//* If there is a lock, nullify it.
	nullifylock();
	//* do priority boosting.
	boostpriority(); //* defined in proc_mlfq.c
      }
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  
  //* Apply MLFQ: Different Time quantum for each queue level (2n + 4)
    //* L0: 4 ticks
    //* L1: 6 ticks
    //* L2: 8 ticks
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
  {
    (myproc()->tq)--; //* Increase process tick.
    if(myproc()->tq <= 0) //*Time Quantum Expired
    { //* If allowed time quantum elapsed,
      //* cprintf("Time Quantum for process %d had been expired!!(level %d, %d ticks)\n",myproc()->pid, myproc()->level, rettq(myproc()));

      //*L2: Time Qunatum Expired -> increase current priority; 
      if(myproc()->level == 2)
      {
        incpriority();
      }	
      else if(myproc()->level == 1 || myproc()->level == 0) //* Time Quantum Expired -> demote current process.
      {
        demoteproc(); //* Definition: proc_mlfq.c
      }
      else //* ERROR
      {
        panic("Time Quantum!\n");
      }
      //yield();
    }
    yield(); //* Changed yield code due to Piazza implementation direction https://piazza.com/class/lf0nppamy5p2hi/post/58
  }

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
