#ifndef H_COMM
#define H_COMM

typedef enum
{
    cmd_msg_t,
    mem_all_msg_t,
    mem_pid_msg_t,
    memkill_msg_t,
} msg_t;

typedef struct command
{
    char *file;
    char **argv;
    char argc;
    char *log;
    char *out;
    char *time;
    _IO_FILE *log_file;
} command_t;
#endif