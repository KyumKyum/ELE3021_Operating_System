//* Wrapper function for syscalls used in Project #2
#include "types.h"
#include "defs.h"
#include "mmu.h" //* struct taskstate, MSGES def in proc.h
#include "param.h" //* NCPU, NOFILE def in proc.h
#include "proc.h"
#include "thread.h" //* Threads

//* list - proc.c
int
sys_list(){
  return list();
}

//* setmemorylimit - proc.c
int
sys_setmemorylimit(void){
  int pid, lim;
  if(argint(0, &pid) < 0)
    return -1;

  if(argint(1, &lim) < 0)
    return -1;

  return setmemorylimit(pid, lim);
}

//*thread_create - thread.c
int
sys_thread_create(void){
  thread_t *thread;
  void*(*start_routine)(void*) = 0;
  void* arg;

  if(argptr(0, (char**)&thread, sizeof(thread_t *)) < 0 || argptr(1, (char**)&(*start_routine), sizeof(void*)) < 0 
		  || argptr(2, (char**)&arg, sizeof(void*)) < 0){
    cprintf("sys_thread_create: read args failed.\n");
    return -1;
  }

  return thread_create(thread, start_routine, arg);
}
