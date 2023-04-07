//* User Program calls syscall 'printproc()'

#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char* argv[])
{
  printproc();
  exit();
}
