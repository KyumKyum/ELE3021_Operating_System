#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char* argv[]){
 //Call all 5 system calls
 
  //yield();
  //getLevel();
  //setPriority(1, 1);
  schedulerLock(2019014266);
  schedulerUnlock(2019014266);

  //__asm__("int $129"); //* schedulerLock()
  //__asm__("int $130"); //* schedulerUnlock()
  //

  printmlfq();
  exit();
}
