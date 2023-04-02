#include "types.h"
#include "defs.h"

void schedulerUnlock(int password){
  cprintf("schedulerUnlock() called! - password: %d\n", password);
  return;
}

int sys_schedulerUnlock(void){
  int password;

  if(argint(0, &password) < 0){
    return -1;
  } 

  schedulerUnlock(password);

  return 0;
}
