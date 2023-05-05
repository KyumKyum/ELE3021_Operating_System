//* Wrapper function for syscalls used in Project #2
#include "types.h"
#include "defs.h"
#include "mmu.h" //* struct taskstate, MSGES def in proc.h
#include "param.h" //* NCPU, NOFILE def in proc.h
#include "proc.h"

//* list - proc.c
int
sys_list(){
  return list();
}
