//* ELE3021 Project #1: Make MLFQ Scheduler
//* Proc.h dedicated for MLFQ Queue

// Per-CPU state
struct cpu {
  uchar apicid;                // Local APIC ID
  struct context *scheduler;   // swtch() here to enter scheduler
  struct taskstate ts;         // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS];   // x86 global descriptor table
  volatile uint started;       // Has the CPU started?
  int ncli;                    // Depth of pushcli nesting.
  int intena;                  // Were interrupts enabled before pushcli?
  struct proc *proc;           // The process running on this cpu or null
};


extern struct cpu cpus[NCPU];
extern int ncpu;

//PAGEBREAK: 17
// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates it.
struct context {
  uint edi; //* Destination Index; Saves destination address when copying 
  uint esi; //* Source Index; Saves source address when copying
  uint ebx; //* Base Register
  uint ebp; //* Base Pointer; Saves the first address of the stack
  uint eip; //* Instruction Pointer; points address of next insruction
};

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };
// Per-process state
//
struct proc {
  uint sz;                     // Size of process memory (bytes)
  pde_t* pgdir;                // Page table
  char *kstack;                // Bottom of kernel stack for this process
  enum procstate state;        // Process state
  int pid;                     // Process ID
  struct proc *parent;         // Parent process
  struct trapframe *tf;        // Trap frame for current syscall
  struct context *context;     // swtch() here to run process
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)
  int stacksize;	       //* Allocated Stack Page Size
  int memlim; 		       //* Memory Limit (Unlimited if 0 assigned)
  int threadnum; 	       //* total thread number
  int isthread;		       //* flag variable shows current process is thread or not.
  struct thread_t *thread;      //* contains thread information.
  int thctr;			//* number of thread created: used for making new thread id.
  //char *tstack;		       //* Top of thread stack for this process;
  //struct thread_t* threads[100]; //* TODO: Need to remove Threads available.
  //int tgttid; 			//* shows target thread to schedule. -1 if there is no thread.
};

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
