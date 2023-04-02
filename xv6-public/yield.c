#include "types.h"
#include "defs.h"

void yieldProc(void){
  cprintf("yieldProc() called!\n");
  return;
}

int sys_yieldProc(void){
  yieldProc();

  return 0;
}
