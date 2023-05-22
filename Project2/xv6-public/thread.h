
//* thread.h
//* Definitions
#define TSTACKSIZE 2000
#define THREADSIZE 4096
#define TMAXPERPROC 100

//*thread_t structure  definition
typedef struct thread_t{
  ushort		tid;
  ushort		pid;
  ushort		exitcalled;
  ushort		occupied;
  void*			retval;
  void*(*start_routine)(void*);
  struct proc*		parent;
  void* 		arg;
}thread_t; 





//* Function Prototypes
int thread_create(thread_t*, void*(*start_routine)(void*), void*);
void thread_exit(void*);
int thread_join(thread_t, void**);
//* Function in service
thread_t thread_self();
int allocthread(thread_t*);
void terminatethread(void*);
void cleanupthread(struct proc*);
int waitthread(thread_t, void**);
void purgethreads(struct proc*, struct proc*);

//* thread syscalls.
int             fetchthread(uint, thread_t*);
int             argthread(int, thread_t*);
int		argvdptr(int, void***, int);
