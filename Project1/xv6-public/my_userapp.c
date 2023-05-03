//User Program implements user system call

#include "types.h"
#include "stat.h"
#include "user.h"

int main (int argc, char *argv[]){
  if(argc <= 1){ // Termination for invalid argurment
    exit();
  };

  if(strcmp(argv[1], "\"user\"") != 0) { // Termination for argument is not same with string "user" 
    exit();
  }

  char *buf = "Hello xv6!";
  int ret_val;
  
  //System Call
  ret_val = myfunction(buf);
  printf(1, "Return Value: 0x%x\n", ret_val);

  exit();
}
