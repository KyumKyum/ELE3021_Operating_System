#include "types.h"
#include "stat.h"
#include "user.h"
#include "thread.h"

void* thread_test(void* data){
  char* threadName = (char*) data; //* Extract data from thread -> Passed by argument.
  printf(0, "[1] Thread Called! Name: %s\n", threadName);
  thread_exit((void*)1);
  //exit();
  return (void*)0;
}

void* thread_test2(void* data){
  char* threadName = (char*)data;
  printf(0, "[2] Thread called! Name: %s\n", threadName);
  thread_exit((void*)1);
  //exit();
  return (void*)0;
}

int
test(void*(*start_routine)(void*), void* data){
  start_routine(data);
  return 0;
}

int
main(){
  thread_t t_thread[3]; //* Thread Declaration.
  int res[3] = {0,0,0}; //* thread result.
  char t1[] = "THREAD_1";
  char t2[] = "THREAD_2";
  //char t3[] = "THREAD_3";

  //printf(0, "thread_test address: %d\n", thread_test);
  //printf(0 , "thread_test2 address: %d\n", &thread_test2);
  
  //printreg(2);
  //test((void*)thread_test, (void*) t1);
  //printreg(5);
  //test((void*)thread_test2, (void*) t2);
  
  //*Thread Creation
  thread_create(&t_thread[0], thread_test, (void*) t1);
  thread_create(&t_thread[1], thread_test2, (void*) t2);
  //thread_create(&t_thread[2], (void*)thread_test2, (void*) t3); 
 
  thread_join(t_thread[0], (void**)&res[0]);
  thread_join(t_thread[1], (void**)&res[1]);
  
  printf(0, "thread_join called: Thread 0 finished with value %d\n", res[0]);
  printf(0, "thread_join called: Thread 1 finished with value %d\n", res[1]);

  exit();
}
