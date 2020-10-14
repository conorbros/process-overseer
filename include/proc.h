#ifndef H_PROC
#define H_PROC

#include <sched.h>
#include "comm.h"

typedef struct proc
{
    pid_t pid;
    char time[26];
    int bytes;
    command_t *cmd;
    struct proc *next;
} proc_entry_t;

#endif