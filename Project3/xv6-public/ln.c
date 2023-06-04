#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  if(argc != 4){
    printf(2, "Usage: ln [-h|-s] old new\n"); //* add symbolic link
    exit();
  }
  if(link(argv[1], argv[2], argv[3]) < 0)
    printf(2, "link %s %s %s: failed\n", argv[1], argv[2], argv[3]); 
  exit();
}
