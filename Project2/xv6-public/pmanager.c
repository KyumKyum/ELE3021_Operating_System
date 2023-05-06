#include "types.h"
#include "user.h"
#include "pmanager.h"

//* Process Manager
int
main(){
  static char buf[BUF_SIZE];
  printf(0, "Initializing pmanager...\n");

  while(recv_cmd(buf, sizeof(buf)) >= 0){

    int res = run_cmd(parse_cmd(buf), buf);

    if (res < 0) //* Error Occured
      goto err; 
    else if(res == 1)
      goto exit;
  }

exit:
  printf(0, "Exiting Pmanager... Bye~\n");
  exit();

err: //* Error Handling
  printf(0, "Pmanager: Unexpected Error Occured while executing command \"%s\", shutting down pmanager.\n", buf);
  return -1;
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

int
run_cmd(int cmd, char *buf){
char* argv[MAX_ARGV]; //* String For Argument Parsing

  switch(cmd){
    case LIST: //* list
      if(list() < 0){
        return -1; //* Unexpected Error
      }
      break;

    case KILL: //* kill
      parse_argument(buf, argv, BUF_SIZE);

      int pid = atoi(argv[0]);
      
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
      //char **argv[10]; //* Maximum 10 arguments.
      //for(int i = 0; i < 10; i++){
      //  find_argument(buf, arg, i+1);
      //}
      break;

    case MEMLIM:
      //* TODO
      break;

    case EXIT: //* exit
      return 1;
      break;

    default: //* No such command
      printf(0, "Pmanager: Unknown Command \"%s\" \n", buf);
  }

  return 0; //* Command Executed Successfully
}

int
parse_cmd(char* cmd){
  if(cmd[0] == 'l' && cmd[1] == 'i' && cmd[2] == 's' && cmd[3] == 't' && (cmd[4] == 0 || cmd[4] == ' ')){
    return LIST; //* Current command is list command.
  }
  else if(cmd[0] == 'k' && cmd[1] == 'i' && cmd[2] == 'l' && cmd[3] == 'l' && (cmd[4] == ' ' || cmd[4] == 0)){
    return KILL; // Current command is kill command
  }
  else if(cmd[0] == 'e' && cmd[1] == 'x' && cmd[2] == 'e' && cmd[3] == 'c' 
		  && cmd[4] == 'u' && cmd[5] == 't' && cmd[6] == 'e' && cmd[7] == ' '){
    return EXEC; // Current command is execute command
  }
  else if(cmd[0] == 'm' && cmd[1] == 'e' && cmd[2] == 'm' && cmd[3] == 'l' 
		  && cmd[4] == 'i' && cmd[5] == 'm' && cmd[6] == ' '){
    return MEMLIM; // Current command is memlim command
  }
  else if(cmd[0] == 'e' && cmd[1] == 'x' && cmd[2] == 'i' && cmd[3] == 't' && (cmd[4] == 0 || cmd[4] == ' ')){
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

char** parse_argument(char* cmd, char** argv, int buf_size){
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
    start = i+1; //* start from next pos
  }

  return argv; 
} 
