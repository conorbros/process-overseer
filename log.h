#ifndef H_LOG
#define H_LOG

#include <sched.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <stdio.h>
#include <stdbool.h>
#include "comm.h"
#include "proc.h"

#define TIME_LEN 26

void get_time();

void print_exec_cmd_attempt(command_t *cmd);

void print_exec_cmd_err(command_t *cmd);

void print_exec_cmd_exit(command_t *cmd, pid_t pid, int status);

void print_exec_cmd_err(command_t *cmd);

void print_exec_cmd_exit(command_t *cmd, pid_t pid, int status);

void print_exec_cmd_sent_signal(command_t *cmd, pid_t pid, int sig);

void print_exec_cmd(command_t *cmd, pid_t pid);

bool pid_in_arr(pid_t pid, pid_t arr[5], int n);

#endif