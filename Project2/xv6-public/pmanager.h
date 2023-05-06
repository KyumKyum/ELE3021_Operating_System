//* Defined Commands
#define LIST 1
#define KILL 2
#define EXEC 3
#define MEMLIM 4
#define EXIT 5

//* Defined constants
#define BUF_SIZE 100
#define MAX_ARGV 10

//* Function Prototypes
int recv_cmd(char*, int); //* receive command from user
int run_cmd(int, char*); //* run current command
int parse_cmd(char*); //* parse command, make it recognizable to pmanager.
char* null_eliminate(char*, int); //* eliminate null at the end of the command.
char** parse_argument(char*, char**, int); //* parse arguments.
