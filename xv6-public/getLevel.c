#include "types.h"
#include "defs.h"


int getLevel(void){
  cprintf("getLevel() called\n");
  return 0;
}

int sys_getLevel(void){
  return getLevel();
}
