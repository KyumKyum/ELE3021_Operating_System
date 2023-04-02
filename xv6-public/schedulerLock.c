#include "types.h"
#include "defs.h"

void schedulerLock(int password){
  cprintf("schedulerLock() called -> password: %d\n", password);
  return;
}

int sys_schedulerLock(void){
  int password;

  if(argint(0, &password) < 0){
    return -1;
  }

  schedulerLock(password);

  return 0;
}
