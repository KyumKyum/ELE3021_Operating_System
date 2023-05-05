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
  printf(2, "> ");
  memset(buf, 0, buf_size);
  gets(buf, buf_size);

  buf = null_terminate(buf, buf_size);
  if(buf[0] == 0){ //* Error: EOF
    return -1;
  }

  return 0;
}

int
run_cmd(int cmd, char *buf){
  switch(cmd){
    case LIST: //* Command List 
      if(list() < 0){
        return -1; //* Unexpected Error
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
    return 1; //* Current command is list command.
  }

  return -1;
}

char* null_terminate(char* cmd, int buf_size){
  for(int i = 0; i < buf_size; i++){
    if(cmd[i] == '\n'){ //* Eleminate this and return cmd.
      cmd[i] = 0;
    }
  }

  return cmd;
}
