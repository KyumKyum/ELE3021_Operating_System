#include "types.h"
#include "defs.h"

//Practicing System call: made simple syscall.

int myfunction(char *str){
  cprintf("%s\n", str); //print the argurment string
  return 0xABCDABCD; //return hex value
}

//Wrapper function for current syscall

int sys_myfunction(void){
  char *str;

  //Decode argument from user space: use argstr
  if (argstr(0, &str) < 0){
    return -1; //Error: Cannot read argument
  }

  return myfunction(str);
}

