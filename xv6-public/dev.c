#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char* argv[]){
 //Call all 5 system calls
 
  //yield();
  //getLevel();
  setPriority(1, 1);
  //schedulerLock(1);
  //schedulerUnlock(2);


  //__asm__("int $129");
  //__asm__("int $130");
  //

  printmlfq();
  exit();
}
