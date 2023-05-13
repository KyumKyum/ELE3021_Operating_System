#include "types.h"
#include "stat.h"
#include "user.h"
#include "thread.h"

void* thread_test(void* data){
  char* threadName = (char*) data; //* Extract data from thread -> Passed by argument.
  
  printf(0, "Thread Called! Name: %s\n", threadName);
  exit();
}

int thread_test2(void* data){
  char* threadName = (char*)data;
  printf(0, "Thread called from test2! Name: %s\n", threadName);
  exit();
}

int
main(){
  thread_t t_thread[3]; //* Thread Declaration.
  char t1[] = "THREAD_1";
  char t2[] = "THREAD_2";
  char t3[] = "THREAD_3";

  //*Thread Creation
  thread_create(&t_thread[0], thread_test, (void*) t1);
  thread_create(&t_thread[1], thread_test, (void*) t2);
  thread_create(&t_thread[2], (void*)thread_test2, (void*) t3); 
  exit();
}
