#include "types.h"
#include "defs.h"
#include "mmu.h"
#include "param.h"
#include "thread.h"
#include "proc.h"

//* Thread inplementation

//* thread_create
int
thread_create(thread_t *thread, void*(*start_routine)(void*), void *arg){
  //* Thread Creation

  memset(thread, 0, sizeof(thread));
  thread->start_routine = start_routine;
  thread->arg = arg;

  if(allocthread(thread) < 0) //* Error: Thread allocation Failed. 
    return -1;

  return 0;
}

//* thread_exit
void
thread_exit(void* ret_val){
  terminatethread(ret_val);
  return;
}

//* thread_join
int
thread_join(thread_t thread, void** retval){
  if(waitthread(thread, retval) < 0){ //* Error: Thread Join Failed.
    cprintf("thread_join failed\n");
    return -1;
  }

  return 0; //* Successfully terminated.
}

