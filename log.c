#include "log.h"

void get_time(char buffer[TIME_LEN])
{
    time_t timer;
    struct tm tm_info;
    timer = time(NULL);
    localtime_r(&timer, &tm_info);
    strftime(buffer, TIME_LEN, "%Y-%m-%d %H:%M:%S", &tm_info);
}

void print_exec_cmd_attempt(command_t *cmd)
{
    char time[TIME_LEN];
    get_time(time);
    fprintf(cmd->log_file, "%s - attempting to execute %s", time, cmd->file);
    for (int i = 0; i < cmd->argc; i++)
    {
        fprintf(cmd->log_file, " %s", cmd->argv[i]);
    }
    fprintf(cmd->log_file, "\n");
}

void print_exec_cmd_err(command_t *cmd)
{
    char time[TIME_LEN];
    get_time(time);
    fprintf(cmd->log_file, "%s - could not execute %s", time, cmd->file);
    for (int i = 0; i < cmd->argc; i++)
    {
        fprintf(cmd->log_file, " %s", cmd->argv[i]);
    }
    fprintf(cmd->log_file, "\n");
}

void print_exec_cmd_exit(command_t *cmd, pid_t pid, int status)
{
    char time[TIME_LEN];
    get_time(time);
    fprintf(cmd->log_file, "%s - %d has terminated with the exit status code %d\n", time, pid, status);
}

void print_exec_cmd_sent_signal(command_t *cmd, pid_t pid, int sig)
{
    char time[TIME_LEN];
    get_time(time);
    if (sig == SIGTERM)
    {
        fprintf(cmd->log_file, "%s - sent SIGTERM to %d\n", time, pid);
    }
    else if (sig == SIGKILL)
    {
        fprintf(cmd->log_file, "%s - sent SIGKILL to %d\n", time, pid);
    }
}

void print_exec_cmd(command_t *cmd, pid_t pid)
{
    char time[TIME_LEN];
    get_time(time);
    fprintf(cmd->log_file, "%s - %s", time, cmd->file);
    for (int i = 0; i < cmd->argc; i++)
    {
        fprintf(cmd->log_file, " %s", cmd->argv[i]);
    }
    fprintf(cmd->log_file, " has been executed with pid %d", pid);
    fprintf(cmd->log_file, "\n");
}

/**
 * @brief Determines if the supplied pid is present in the supplied array
 * 
 * @param pid to search for
 * @param arr to search
 * @param n array length
 * @return true 
 * @return false 
 */
bool pid_in_arr(pid_t pid, pid_t arr[5], int n)
{
    for (int i = 0; i < n; i++)
    {
        if (arr[i] == pid)
        {
            return true;
        }
    }
    return false;
}
