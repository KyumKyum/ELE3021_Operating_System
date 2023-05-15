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
  
  thread->start_routine = start_routine;
  thread->arg = arg;

  //cprintf("Size of process: %d\n", sizeof(struct proc));
  //cprintf("Size of thread: %d\n", sizeof(thread_t));
  //cprintf("Before bind\n");
  //cprintf("Args: %s", (char*)arg);
  if(allocthread(thread) < 0) //* Error: Thread allocation Failed. 
    return -1;

  return 0;
}

//* thread_exit
void
thread_exit(void* ret_val){
  //* Thread Exit

}

