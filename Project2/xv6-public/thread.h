
//* thread.h
//* Definitions
#define TSTACKSIZE 2000
#define THREADSIZE 4096
#define TMAXPERPROC 100

//*thread_t structure  definition
typedef struct thread_t{
  ushort		tid;
  ushort		pid;
  void*(*start_routine)(void*);
  void* 		arg;
  struct context* 	context; // * Thread's context.
  struct trapframe* 	tf; // * Thread's Trap Frame 
}thread_t; 





//* Function Prototypes
int thread_create(thread_t*, void*(*start_routine)(void*), void*);
//void thread_exit(void*);
thread_t thread_self();
int allocthread(thread_t*);
struct thread_t* threadinit(thread_t*);
