
//* thread.h
//*thread_t structure  definition
typedef struct thread_t{
  int			tid;
  void*(*start_routine)(void*);
  void* 		arg;
  int			retval; //* Return Value - defined by thread_exit()
  int 			terminated; //* 0 if thread is valid, 1 if thread had been terminated
  struct proc*		parent;
  struct proc* 		p;
  struct thread_t*	next;
  struct thread_t*	prev;
}thread_t;





//* Function Prototypes
int thread_create(thread_t*, void*(*start_routine)(void*), void*);
//void thread_exit(void*);
thread_t thread_self();
int bindthread(thread_t*);
int allocthread(struct proc*, thread_t*);
thread_t* threadinit(thread_t*);
