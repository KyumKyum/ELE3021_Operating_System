#include "types.h"
#include "defs.h"  
#include "mmu.h" //* struct taskstate, MSGES def in proc.h 
#include "param.h" //* NCPU, NOFILE def in proc.h
#include "proc.h"

//* Syscall for mlfq

//* ==================== REQUIRED SYSTEM CALL ====================
//* getLevel()
int 
getLevel(void)
{
  if(myproc())
  {
    cprintf("Current Process Level: %d\n", myproc()->level);
    return myproc()->level; // * Return current process level.
  }
  else // * Error Case;
    return -1;
  return 0;
}

//* setPriority()
/*void 
setPriority(int pid, int priority)
{
  //cprintf("setPriority() called, pid: %d, priority: %d\n", pid, priority);
  struct proc* p = getproc(pid);

  if(p->pid == -1) // * There are no such process
  {
    cprintf("setPriority(): There are no such process having pid %d.\n", pid);
  }
  else
  {
    cprintf("FOUNDED: Pid - %d\n", p->pid);
  }
  return;
}*/

//* schedulerLock()
/*void 
schedulerLock(int password)
{
  cprintf("schedulerLock() called -> password: %d\n", password);
  return;
}*/

//* schedulerUnlock()
/*void 
schedulerUnlock(int password)
{
  cprintf("schedulerUnlock() called! - password: %d\n", password);
  return;
}*/


//* Wrapper Function for current systemcall

//* getLevel()
int 
sys_getLevel(void)
{
  return getLevel();
}

//* setPriority()
int 
sys_setPriority(void)
{
  int pid, priority;

  if(argint(0, &pid) < 0){
    return -1;
  }

  if(argint(1, &priority) < 0){
    return -1;
  }

  setPriority(pid, priority);
  return 0;
}

//* schedulerLock()
int 
sys_schedulerLock(void)
{
  int password;

  if(argint(0, &password) < 0){
    return -1;
  }

  schedulerLock(password);

  return 0;
}

//* schedulerUnlock()
int 
sys_schedulerUnlock(void)
{
  int password;

  if(argint(0, &password) < 0){
    return -1;
  }

  schedulerUnlock(password);

  return 0;
}

//* ==================== SYSTEM CALL ====================

int 
rettq(struct proc *p) //* Return time quantum for each queue.
{
  if(p->lock == LOCKED)
  { //*Locked process, gives 100 timequantum as max.
    return 100;
  }
  else
  {
    return (2 * p->level) + 4;
  }
}


