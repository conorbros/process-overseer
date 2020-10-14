#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include "comm.h"
#include "proc_map.h"
#include "log.h"
#include "proc.h"
#include "thread_pool.h"

#define BACKLOG 10
#define COMMAND_NOT_RUNNABLE 127
#define KILL_WAIT 5
#define TERM_WAIT 10

int quitting = 0;

/**
 * @brief same function as asprintf but without leaking memory
 * 
 */
#define sasprintf(write_to, ...)                \
    {                                           \
        char *tmp_string_for_extend = write_to; \
        asprintf(&(write_to), __VA_ARGS__);     \
        free(tmp_string_for_extend);            \
    }

int sockfd;

pthread_mutex_t proc_mutex = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
proc_entry_t *procs = NULL;

void lock_proc_mutex()
{
    if (pthread_mutex_lock(&proc_mutex) != 0)
    {
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }
}

void unlock_proc_mutex()
{
    if (pthread_mutex_unlock(&proc_mutex) != 0)
    {
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief Kills all currently running processes
 * 
 */
void kill_all_procs()
{
    pid_t pids[NUM_HANDLER_THREADS];
    int pids_n = 0;
    lock_proc_mutex();
    proc_entry_t *curr = procs;
    while (curr != NULL)
    {
        if (!pid_in_arr(curr->pid, pids, pids_n))
        {
            kill(curr->pid, SIGKILL);
            pids[pids_n] = curr->pid;
            pids_n++;
        }
        curr = curr->next;
    }
    unlock_proc_mutex();
}

/**
 * @brief Removes all procs with a pid matching the supplied one from the linked list
 * 
 * @param pid 
 */
void remove_proc(pid_t pid)
{
    proc_entry_t *prev = NULL;
    lock_proc_mutex();
    proc_entry_t *tmp = procs;
    while (tmp != NULL && tmp->pid == pid)
    {
        procs = tmp->next;
        free(tmp);
        tmp = procs;
    }

    while (tmp != NULL)
    {
        while (tmp != NULL && tmp->pid != pid)
        {
            prev = tmp;
            tmp = tmp->next;
        }

        if (tmp == NULL)
            break;

        prev->next = tmp->next;
        free(tmp);
        tmp = prev->next;
    }
    unlock_proc_mutex();
}

/**
 * @brief Adds a memory entry record to the linked list
 * 
 * @param pid 
 * @param bytes 
 * @param cmd 
 * @return proc_entry_t* 
 */
proc_entry_t *add_proc_mem_entry(pid_t pid, int bytes, command_t *cmd)
{
    proc_entry_t *new_proc = NULL;
    new_proc = (proc_entry_t *)malloc(sizeof(proc_entry_t));
    if (!new_proc)
    {
        fprintf(stderr, "add_proc_mem_entry: out of memory\n");
        exit(EXIT_FAILURE);
    }

    new_proc->pid = pid;
    char time[TIME_LEN];
    get_time(time);
    strcpy(new_proc->time, time);
    new_proc->bytes = bytes;
    new_proc->cmd = cmd;
    new_proc->next = NULL;

    lock_proc_mutex();

    new_proc->next = procs;
    procs = new_proc;

    unlock_proc_mutex();
    return new_proc;
}

void exec_cmd(command_t *cmd)
{
    bool end_manually = false;
    int status;
    time_t start, end;
    double elapsed;

    int term_wait;

    if (cmd->time[0] == '\0')
        term_wait = TERM_WAIT;
    else
        term_wait = atoi(cmd->time);

    print_exec_cmd_attempt(cmd);

    // Using stdout
    bool redirect;
    FILE *out_file;

    if (cmd->out[0] == '\0')
    {
        redirect = false;
    }
    else
    {
        redirect = true;
        out_file = fopen(cmd->out, "a+");
        if (out_file == NULL)
        {
            perror("open");
            fprintf(stderr, "Outfile error\n");
            exit(EXIT_FAILURE);
        }
    }

    char *arg_arr[cmd->argc + 1];
    // First argument is file path
    arg_arr[0] = cmd->file;
    for (int i = 1; i < cmd->argc + 1; i++)
    {
        arg_arr[i] = cmd->argv[i - 1];
    }
    // null terminate char array
    arg_arr[cmd->argc + 1] = NULL;

    pid_t ret = fork();
    if (ret < 0)
    {
        fprintf(stderr, "fork() failed\n");
        exit(EXIT_FAILURE);
    }
    else if (ret == 0)
    {
        if (redirect)
        {
            dup2(fileno(out_file), STDOUT_FILENO);
            dup2(fileno(out_file), STDERR_FILENO);
        }

        if (execv(cmd->file, arg_arr) == -1)
        {
            exit(COMMAND_NOT_RUNNABLE);
        }
    }

    // Sleep to let the child process have time to start
    sleep(1);
    start = time(NULL);
    bool started = true;
    while (waitpid(ret, &status, WNOHANG) == 0)
    {
        if (started)
        {
            print_exec_cmd(cmd, ret);
            started = false;
        }
        end = time(NULL);
        elapsed = difftime(end, start);
        if (elapsed > term_wait)
        {
            end_manually = true;
            break;
        }
        sleep(1);
        int bytes = get_bytes_proc_using(ret);
        add_proc_mem_entry(ret, bytes, cmd);
    }
    remove_proc(ret);

    // Has timed out, must end manually
    if (end_manually)
    {
        kill(ret, SIGTERM);
        print_exec_cmd_sent_signal(cmd, ret, SIGTERM);

        start = time(NULL);
        do
        {
            end = time(NULL);
            elapsed = difftime(end, start);
            sleep(1);
        } while (kill(ret, 0) == 0 && elapsed < KILL_WAIT);

        // If the process is still running 5 seconds after sending SIGTERM, must kill
        if (waitpid(ret, &status, WNOHANG) == 0)
        {
            kill(ret, SIGKILL);
            print_exec_cmd_sent_signal(cmd, ret, SIGKILL);
        }
    }

    if ((WIFEXITED(status) || WIFSIGNALED(status)) && !quitting)
    {
        int es = WEXITSTATUS(status);
        if (es != COMMAND_NOT_RUNNABLE)
        {
            print_exec_cmd_exit(cmd, ret, es);
        }
        else
        {
            print_exec_cmd_err(cmd);
        }
    }
    if (redirect)
    {
        if (fclose(out_file) == -1)
        {
            perror("fclose");
            exit(EXIT_FAILURE);
        }
    }
}

/**
 * @brief Receives a command field from the controller
 * 
 * @param conn_sockfd 
 * @param field 
 */
void recv_cmd_field(int conn_sockfd, char **field)
{
    int recv_len;
    if (recv(conn_sockfd, &recv_len, sizeof(int), PF_UNSPEC) == -1)
    {
        perror("recv");
        exit(EXIT_FAILURE);
    }
    const int len = ntohs(recv_len);

    char recv_char[len];
    if (recv(conn_sockfd, &recv_char, sizeof(char) * len, PF_UNSPEC) == -1)
    {
        perror("recv");
        exit(EXIT_FAILURE);
    }
    *field = malloc(len * sizeof(char));
    memcpy(*field, recv_char, len);
}

void close_sock(int conn_sockfd)
{
    shutdown(conn_sockfd, SHUT_RDWR);
    close(conn_sockfd);
}

void free_cmd(command_t *cmd)
{
    free(cmd->file);
    free(cmd->log);
    free(cmd->out);
    free(cmd->time);
    for (int i = 0; i < cmd->argc; i++)
    {
        free(cmd->argv[i]);
    }
    free(cmd->argv);
    free(cmd);
}

/**
 * @brief Recieves and executes a command from a controller 
 * 
 * @param conn_sockfd 
 */
void handle_cmd(int conn_sockfd)
{
    int recv_val;
    char *file = NULL, *log = NULL, *out = NULL, *time = NULL;
    command_t *cmd = malloc(sizeof(command_t));
    cmd->argc = 0;

    recv_cmd_field(conn_sockfd, &file);
    cmd->file = strdup(file);
    if (file)
        free(file);

    recv_cmd_field(conn_sockfd, &log);
    cmd->log = strdup(log);
    if (log)
        free(log);

    recv_cmd_field(conn_sockfd, &out);
    cmd->out = strdup(out);
    if (out)
        free(out);

    recv_cmd_field(conn_sockfd, &time);
    cmd->time = strdup(time);
    if (time)
        free(time);

    if (recv(conn_sockfd, &recv_val, sizeof(int), PF_UNSPEC) == -1)
    {
        perror("recv");
        exit(EXIT_FAILURE);
    }

    cmd->argc = ntohs(recv_val);

    cmd->argv = malloc(sizeof(char *) * cmd->argc);
    // Receive command args
    if (cmd->argc > 0)
    {
        for (int i = 0; i < cmd->argc; i++)
        {
            recv_cmd_field(conn_sockfd, &cmd->argv[i]);
        }
    }

    close_sock(conn_sockfd);

    if (cmd->log[0] == '\0')
    {
        cmd->log_file = stdout;
    }
    else
    {
        FILE *logfile;
        logfile = fopen(cmd->log, "a");
        if (logfile == NULL)
        {
            fprintf(stderr, "Logfile error\n");
            exit(EXIT_FAILURE);
        }
        cmd->log_file = logfile;
    }

    exec_cmd(cmd);

    // If process was not printing to stdout close the logfile
    if (cmd->log_file != stdout)
    {
        fclose(cmd->log_file);
    }

    free_cmd(cmd);
}

/**
 * @brief Sends a memory record entry
 * 
 * @param conn_sockfd 
 * @param entry 
 */
void send_mem_entry(int conn_sockfd, char *entry)
{
    const int len = strlen(entry) + 1;
    const int val = htons(len);
    if (send(conn_sockfd, &val, sizeof(int), PF_UNSPEC) == -1)
    {
        perror("send");
        exit(EXIT_FAILURE);
    }

    if (send(conn_sockfd, entry, len, PF_UNSPEC) == -1)
    {
        perror("send");
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief Gets the latest memory record entries for each currently running process and returns to the controller
 * 
 * @param conn_sockfd 
 */
void handle_mem_all(int conn_sockfd)
{
    pid_t pids[NUM_HANDLER_THREADS];
    char *mem_entries[NUM_HANDLER_THREADS];
    int pids_n = 0, mem_n = 0;

    lock_proc_mutex();
    proc_entry_t *curr = procs;
    while (curr != NULL)
    {
        if (!pid_in_arr(curr->pid, pids, pids_n))
        {
            mem_entries[mem_n] = NULL;
            sasprintf(mem_entries[mem_n], "%d %d %s", curr->pid, curr->bytes, curr->cmd->file);

            for (int i = 0; i < curr->cmd->argc; i++)
            {
                sasprintf(mem_entries[mem_n], "%s %s", mem_entries[mem_n], curr->cmd->argv[i])
            }

            pids[pids_n] = curr->pid;
            pids_n++;
            mem_n++;
        }
        curr = curr->next;
    }

    unlock_proc_mutex();

    int send_val = htons(mem_n);
    if (send(conn_sockfd, &send_val, sizeof(int), PF_UNSPEC) == -1)
    {
        perror("send");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < mem_n; i++)
    {
        send_mem_entry(conn_sockfd, mem_entries[i]);
        free(mem_entries[i]);
    }

    close_sock(conn_sockfd);
}

/**
 * @brief Gets all memory entry records for the process specified by the supplied pid and returns to the controller
 * 
 * @param conn_sockfd 
 */
void handle_mem_pid(int conn_sockfd)
{

    pid_t pid;
    if (recv(conn_sockfd, &pid, sizeof(int), PF_UNSPEC) == -1)
    {
        perror("recv");
        exit(EXIT_FAILURE);
    }
    pid = ntohs(pid);

    lock_proc_mutex();
    proc_entry_t *curr = procs;
    char **mem_entrys = (char **)malloc(0 * sizeof(char *));
    int mem_n = 0;

    while (curr != NULL)
    {
        if (curr->pid == pid)
        {
            mem_entrys = (char **)realloc(mem_entrys, (mem_n + 1) * sizeof(char *));
            size_t needed = snprintf(NULL, 0, "%s %d\n", curr->time, curr->bytes) + 1;
            mem_entrys[mem_n] = malloc(needed);
            sprintf(mem_entrys[mem_n], "%s %d", curr->time, curr->bytes);
            mem_n++;
        }
        curr = curr->next;
    }

    unlock_proc_mutex();

    int send_val = htons(mem_n);
    if (send(conn_sockfd, &send_val, sizeof(int), PF_UNSPEC) == -1)
    {
        perror("send");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < mem_n; i++)
    {
        send_mem_entry(conn_sockfd, mem_entrys[i]);
        free(mem_entrys[i]);
    }
    free(mem_entrys);

    close_sock(conn_sockfd);
}

/**
 * @brief Receives a memkill message from the controller and sends SIGKILL to processes over the 
 * 
 * @param conn_sockfd 
 */
void handle_memkill(int conn_sockfd)
{
    int len;
    if (recv(conn_sockfd, &len, sizeof(int), PF_UNSPEC) == -1)
    {
        perror("recv");
        exit(EXIT_FAILURE);
    }
    len = ntohs(len);

    char mem_kill_c[len];
    if (recv(conn_sockfd, &mem_kill_c, sizeof(char) * len, PF_UNSPEC) == -1)
    {
        perror("recv");
        exit(EXIT_FAILURE);
    }
    close_sock(conn_sockfd);

    float mem_threshold = strtof(mem_kill_c, NULL);

    struct sysinfo sys_info;
    if (sysinfo(&sys_info) == -1)
    {
        perror("sysinfo");
        exit(EXIT_FAILURE);
    }
    unsigned long total_mem = sys_info.totalram;

    pid_t pids[NUM_HANDLER_THREADS], to_kill[NUM_HANDLER_THREADS];
    int pids_n = 0, to_kill_n = 0;

    lock_proc_mutex();
    proc_entry_t *curr = procs;

    while (curr != NULL)
    {
        if (!pid_in_arr(curr->pid, pids, pids_n))
        {
            double using = ((double)curr->bytes / (double)total_mem) * 100;
            if (using > (double)mem_threshold)
            {
                to_kill[to_kill_n] = curr->pid;
                pids[pids_n] = curr->pid;
                to_kill_n++;
                pids_n++;
            }
        }
        curr = curr->next;
    }

    unlock_proc_mutex();

    for (int i = 0; i < to_kill_n; i++)
    {
        kill(to_kill[i], SIGKILL);
        sleep(1);
    }
}

/**
 * @brief Handles a connection to the overseer
 * 
 * @param arg 
 */
void handle_conn(void *arg)
{
    int val;
    int *conn_sockfd = arg;
    if (recv(*conn_sockfd, &val, sizeof(int), PF_UNSPEC) == -1)
    {
        perror("recv");
        exit(EXIT_FAILURE);
    }
    msg_t msg = ntohs(val);

    if (msg == cmd_msg_t)
    {
        handle_cmd(*conn_sockfd);
    }
    else if (msg == mem_all_msg_t)
    {
        handle_mem_all(*conn_sockfd);
    }
    else if (msg == mem_pid_msg_t)
    {
        handle_mem_pid(*conn_sockfd);
    }
    else if (msg == memkill_msg_t)
    {
        handle_memkill(*conn_sockfd);
    }
    free(arg);
}

/**
 * @brief Handles a SIGINT signal
 * 
 */
void interrupt_handler()
{
    quitting = 1;
    kill_all_procs();
    quit_threads();
    close_sock(sockfd);
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
    int new_fd;
    struct sockaddr_in my_addr;
    struct sockaddr_in their_addr;
    socklen_t sin_size;
    int port;

    signal(SIGINT, interrupt_handler);

    if (argc < 2)
    {
        fprintf(stderr, "Usage: <port>\n");
        return 1;
    }
    port = atoi(argv[1]);

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    init_threadpool();

    int opt_enable = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt_enable, sizeof(opt_enable));
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &opt_enable, sizeof(opt_enable));

    memset(&my_addr, 0, sizeof(my_addr));

    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1)
    {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(sockfd, BACKLOG) == -1)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    char time[TIME_LEN];
    while (1)
    {
        sin_size = sizeof(struct sockaddr_in);
        if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size)) == -1)
        {
            perror("accept");
            continue;
        }

        void *memory = malloc(sizeof(int));
        int *fd = (int *)memory;
        *fd = new_fd;

        get_time(time);
        printf("%s - connection received from %s\n", time, inet_ntoa(their_addr.sin_addr));

        add_request(handle_conn, fd);
    }
}