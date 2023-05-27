#include "types.h"
#include "user.h"
#include "pmanager.h"

//* Process Manager
int
main(){
  static char buf[BUF_SIZE];
  printf(0, "Initializing pmanager...\n");

  while(recv_cmd(buf, sizeof(buf)) >= 0){

    int cmd = parse_cmd(buf);

    if(cmd == 5)
      goto exit; //* Exit!
    if(forkproc() == 0){
      run_cmd(cmd, buf);
    }
    wait();
  }

exit:
  printf(0, "Exiting Pmanager... Bye Bye~\n");
  exit();
}



int
recv_cmd(char *buf, int buf_size){
  printf(2, "[PMANAGER] > ");
  memset(buf, 0, buf_size);
  gets(buf, buf_size);

  buf = null_eliminate(buf, buf_size);
  if(buf[0] == 0){ //* Error: EOF
    return -1;
  }

  return 0;
}

void
run_cmd(int cmd, char *buf){
char* argv[MAX_ARGV]; //* String For Argument Parsing
int pid;
int rear = 0; //* Indicates the last element. e.g.)  execute's stacksize.

  switch(cmd){
    case LIST: //* list
      
      if(list() < 0){
	printf(0, "Pmanager: Unexpected Error Occured while executing command \"%s\", shutting down pmanager.\n", buf);
        exit(); //* Unexpected Error
      }
      wait();
      break;

    case KILL: //* kill
      parse_argument(buf, argv, BUF_SIZE, &rear);

      pid = atoi(argv[0]);
      
      if(pid <= 0){ //* Invalid pid
        printf(0, "Invalid Pid. Pid must be integer bigger than 0\n");
      	break;
      }

      //* Kill process
      if(kill(pid) < 0){
        printf(0, "Kill command failed; There are no such process with pid %d\n", pid);
      }
      else {
        printf(0, "Successfully killed proccess %d\n", pid);
      }
      break;

    case EXEC:
      //* Execute

      parse_argument(buf, argv, BUF_SIZE, &rear);
      int stacksize = atoi(argv[rear]); //* End of argument: stack size

      argv[rear] = 0; //* Remove current stacksize argument
      exec2(argv[0], argv, stacksize); // argv[0]: path, argv[1]: stacksize
	//wait();
      break;

    case MEMLIM:
      parse_argument(buf, argv, BUF_SIZE, &rear);

      pid = atoi(argv[0]);
      int lim = atoi(argv[1]);
      
      if(setmemorylimit(pid, lim) < 0){
        printf(0, "REJECTED: No such pid or limit is less than it's allocation\n");
      }
      break;

    case EXIT: //* exit
      exit();
      break;

    default: //* No such command
      printf(0, "Pmanager: Unknown Command\n");
  }

exit(); //* Command Executed Successfully
  
}

int
parse_cmd(char* cmd){
  if(cmd[0] == 'l' && cmd[1] == 'i' && cmd[2] == 's' && cmd[3] == 't' && (cmd[4] == '\n' || cmd[4] == ' ')){
    return LIST; //* Current command is list command.
  }
  else if(cmd[0] == 'k' && cmd[1] == 'i' && cmd[2] == 'l' && cmd[3] == 'l' && (cmd[4] == '\n' || cmd[4] == ' ')){
    return KILL; // Current command is kill command
  }
  else if(cmd[0] == 'e' && cmd[1] == 'x' && cmd[2] == 'e' && cmd[3] == 'c' 
		  && cmd[4] == 'u' && cmd[5] == 't' && cmd[6] == 'e' && (cmd[7] == '\n' || cmd[7] == ' ')){
    return EXEC; // Current command is execute command
  }
  else if(cmd[0] == 'm' && cmd[1] == 'e' && cmd[2] == 'm' && cmd[3] == 'l' 
		  && cmd[4] == 'i' && cmd[5] == 'm' && (cmd[6] == '\n' || cmd[6] == ' ')){
    return MEMLIM; // Current command is memlim command
  }
  else if(cmd[0] == 'e' && cmd[1] == 'x' && cmd[2] == 'i' && cmd[3] == 't' && (cmd[4] == '\n' || cmd[4] == ' ')){
    return EXIT; // Current command is exit command
  }
  return -1;
}

char* null_eliminate(char* cmd, int buf_size){
  int start = 0;
  int mv = 0;
  int idx = 0;
  //* Eliminate meaningless whitespaces infront of command.
     
  while(idx < buf_size){
    if(cmd[idx] == ' '){ //* meaningless white space
      start++;
      idx++;
      continue;
    }

    if(start == 0) //* no need to eliminate
      break;

    //* Eliminate all commands
    for(int j = start; j < buf_size; j++){
      cmd[mv] = cmd[j]; //* move command;
      cmd[j] = 0;

      if(cmd[mv] == '\n') //* moved all command; break.
        break;

      mv++;
    }

    break; //* Move Complete
  }
  
  /*for(int i = 0; i < buf_size; i++){
    if(cmd[i] == '\n'){ // * Eleminate this and return cmd.
      cmd[i] = 0;
    }
  }*/

  return cmd;
}

char** parse_argument(char* cmd, char** argv, int buf_size, int *rear){
  int argvsz = 0; //* argument size
  int start = 0; //* Argument starts
  int argvnum = 0;//* number of arguments

  //* Find argument starting place.
  while(start < buf_size){
    if(cmd[start++] != ' '){ // * skip the command part
      continue;
    }

    // * next part will be starting point of argument
    break; // * starting position found.
  }


  for(int i = start; i < buf_size; i++){
    if(cmd[i] != ' ' && cmd[i] != '\n'){ //* Argument
      argvsz++;
      continue;
    }
    //* Whitespace found: End of argument.
    if(argvsz == 0) //* no argument size: skip.
      continue;

    argv[argvnum] = malloc(argvsz);
    for(int tgt = 0; tgt < argvsz; tgt++){
      argv[argvnum][tgt] = cmd[start + tgt];
    }

    if(cmd[i] == '\n') //* if current position is 0, it is endpoint. 
      break;

    argvsz = 0;
    argvnum++;
    (*rear)++; //* rear moved
    start = i+1; //* start from next pos
  }

  return argv; 
}

int forkproc(){
  int pid;

  pid = fork();
  
  if(pid < 0)
    return -1;

  return pid;
}
