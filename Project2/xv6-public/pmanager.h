//* Defined Commands
#define LIST 1
#define KILL 2
#define EXEC 3
#define MEMLIM 4
#define EXIT 5

//* Defined constants
#define BUF_SIZE 100

//* Function Prototypes
int recv_cmd(char*, int);
int run_cmd(int, char*);
int parse_cmd(char*);
char* null_terminate(char*, int);
