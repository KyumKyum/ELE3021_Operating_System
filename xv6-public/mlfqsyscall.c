#include "types.h"
#include "defs.h"  
#include "mmu.h" //* struct taskstate, MSGES def in proc.h 
#include "param.h" //* NCPU, NOFILE def in proc.h
#include "proc.h"

//* Wrapper Function for current systemcall

//* yield()
int sys_yield (void)
{
  cprintf("yield() called in proc.c\n");
  yield();
  return 0;
}

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

//* printmlfq()
int sys_printmlfq(void)
{
  printmlfq();
  exit();
  return 0;
}

//* printproc()
int sys_printproc(void)
{
  printproc();
  exit();
  return 0;
}


