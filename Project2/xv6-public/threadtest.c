#include "types.h"
#include "stat.h"
#include "fcntl.h"
#include "user.h"
#include "mmu.h"
#include "x86.h"


int thread_create2(void (*start_routine)(void *), void* arg1, void* arg2)
{
  void* stack;
  stack = malloc(PGSIZE);

  return allocthread2(start_routine, arg1, arg2, stack);
}

