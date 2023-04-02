#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char* argv[]){
 //Call all 5 system calls
 
  yieldProc();
  getLevel();
  setPriority(1, 2);
  schedulerLock(1);
  schedulerUnlock(2);

  exit();
}
