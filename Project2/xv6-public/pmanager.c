#include "types.h"
#include "user.h"
#include "pmanager.h"

//* Process Manager
int
main(){
  static char buf[BUF_SIZE];
  printf(0, "Initializing pmanager...\n");

  while(recv_cmd(buf, sizeof(buf)) >= 0){

    if(run_cmd(parse_cmd(buf), buf) < 0){ //* Error Occured
      goto err; 
    }
  }

  return 0;

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
char arg[BUF_SIZE]; //* String For Argument Parsing

  switch(cmd){
    case LIST: //* list
      if(list() < 0){
        return -1; //* Unexpected Error
      }
      break;

    case KILL: //* kill
      int pid = atoi(find_argument(buf, arg, 1));
      
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

    default: //* No such command
      printf(0, "Pmanager: Unknown Command \"%s\" \n", buf);
  }

  return 0; //* Command Executed Successfully
}

int
parse_cmd(char* cmd){
  if(cmd[0] == 'l' && cmd[1] == 'i' && cmd[2] == 's' && cmd[3] == 't'){
    return LIST; //* Current command is list command.
  }
  else if(cmd[0] == 'k' && cmd[1] == 'i' && cmd[2] == 'l' && cmd[3] == 'l'){
    return KILL; // Current command is kill command
  }

  return -1;
}

char* null_eliminate(char* cmd, int buf_size){
  for(int i = 0; i < buf_size; i++){
    if(cmd[i] == '\n'){ //* Eleminate this and return cmd.
      cmd[i] = 0;
    }
  }

  return cmd;
}

char* find_argument(char* cmd, char* arg, int idx){
  int tgt = 0; //* Target Argument
  int argidx = 0; //* Argument index

 for(int i = 0; i < BUF_SIZE; i++){
   if(cmd[i] != ' ') 
     continue;

   //* Whitespace found: check if current arguemt is sth we found
   if(++tgt != idx)
     continue; //* Not an argument we are targeting.

  //* Argument Found
  for(int ch = i+1; ch < BUF_SIZE; ch++){
    //* Copy argument until there is an whitespace.
    if(cmd[ch] == ' '){ //* Whitespace found: end of current argument
      return arg; //* Return Argument
    }

    arg[argidx++] = cmd[ch]; //* Copy current character.
  } 
 }

return arg; 
} 
