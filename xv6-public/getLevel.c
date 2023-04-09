#include "types.h"
#include "defs.h"  
#include "mmu.h" //* struct taskstate, MSGES def in proc.h 
#include "param.h" //* NCPU, NOFILE def in proc.h
#include "proc.h"

int getLevel(void){
  if(myproc())
  {
    cprintf("Current Process Level: %d\n", myproc()->level);
    return myproc()->level; // * Return current process level.
  }
  else // * Error Case;
    return -1;
  return 0;
}

int sys_getLevel(void){
  return getLevel();
}
