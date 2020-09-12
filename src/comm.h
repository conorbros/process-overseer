typedef enum
{
    cmd_msg_t,
    mem_msg_t,
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

void print_cmd(command_t *cmd)
{
    printf("file %s\n", cmd->file);
    printf("log: %s\n", cmd->log);
    printf("out: %s\n", cmd->out);
    printf("time: %s\n", cmd->time);
    for (int i = 0; i < cmd->argc; i++)
    {
        printf("arg: %s\n", cmd->argv[i]);
    }
}