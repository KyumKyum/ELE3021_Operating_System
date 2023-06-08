#include "types.h"
#include "user.h"
#include "fcntl.h"

#define N 100


void
save(void)
{
    int fd;
    char* str = "Hello World!!!!!\n";

    fd = open("filetest", O_CREATE | O_RDWR);
    if(fd >= 0) {
        printf(0, "ok: file creation succeed\n");
    } else {
        printf(0, "error: file creation failed\n");
        exit();
    }

    int size = sizeof(str);
    if(write(fd, &str, size) != size){
        printf(0, "error: write failed\n");
        exit();
    }
    printf(0, "write ok\n");
    sync();
    close(fd);
}

void
modify(void)
{
  int fd;
  char* str = "New String\n";

  fd = open("filetest", O_WRONLY);
  
  if(fd < 0){
    printf(0,"error: write failed in modify\n");
  }

  int size = sizeof(str);

  if(write(fd, &str, size) != size){
    printf(0,"write failed in modify.\n");
    exit();
  }

  printf(0, "write ok\n");
  //sync();
  close(fd);
	 
}

void
load(void)
{
    int fd;
    char* str;

    fd = open("filetest", O_RDONLY);
    if(fd >= 0) {
        printf(0, "ok: read file succeed\n");
    } else {
        printf(0, "error: read file failed\n");
        exit();
    }

    int size = sizeof(str);
    if(read(fd, &str, size) != size){
        printf(0, "error: read string in file failed\n");
        exit();
    }
    printf(0, "file content (string): %s", str);
    printf(0, "read ok\n");
    //sync();
    close(fd);
}

int
main(void)
{
    save();
    modify();
    load();

    exit();
}
