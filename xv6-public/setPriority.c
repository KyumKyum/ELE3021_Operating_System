#include "types.h"
#include "defs.h"

void setPriority(int pid, int priority){
  cprintf("setPriority() called, pid: %d, priority: %d\n", pid, priority);
  return;
}

int sys_setPriority(void){
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
