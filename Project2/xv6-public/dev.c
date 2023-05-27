#include "types.h"
#include "stat.h"
#include "user.h"
#include "thread.h"

void* thread_test(void* data){
  char* threadName = (char*) data; //* Extract data from thread -> Passed by argument.
  printf(0, "[1] Thread Called! Name: %s\n", threadName);
  thread_exit((void*)1);
  return (void*)0;
}

void* thread_test2(void* data){
  char* threadName = (char*)data;
  printf(0, "[2] Thread called! Name: %s\n", threadName);
  fork();
  thread_exit((void*)1);
  return (void*)0;
}

int
test(void** data){
  int *val =  (int*)777;
  *data = (void*)val;
  return 0;
}

int
main(){
  thread_t t_thread[3]; //* Thread Declaration.
  void* res[3]; //* thread result.
  char t1[] = "THREAD_1";
  char t2[] = "THREAD_2";
  
  //*Thread Creation
  thread_create(&t_thread[0], thread_test, (void*) t1);
  thread_create(&t_thread[1], thread_test2, (void*) t2);
 
  thread_join(t_thread[0], &res[0]);
  thread_join(t_thread[1], &res[1]);
  
  printf(0, "thread_join called: Thread 0 finished with value %d\n", *(int*)res[0]);
  printf(0, "thread_join called: Thread 1 finished with value %d\n", *(int*)res[1]);

  exit();
}
