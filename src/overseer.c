#define _GNU_SOURCE
#include <stdio.h>   /* standard I/O routines                     */
#include <pthread.h> /* pthread functions and data structures     */
#include <stdlib.h>  /* rand() and srand() functions              */
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include "comm.h"

#define NUM_HANDLER_THREADS 5
#define BACKLOG 10

struct request
{
    void (*func)(void *);
    void *data;
    struct request *next;
};

typedef struct process
{
    pid_t pid;
    char time[26];
    struct process *next;
} process_t;

int sockfd;

pthread_mutex_t request_mutex;
pthread_mutex_t quit_mutex;

pthread_mutex_t process_mutex;

pthread_cond_t got_request;

process_t *processes = NULL;
process_t *last_process = NULL;
int num_processes = 0;

struct request *requests = NULL;
struct request *last_request = NULL;
int num_requests = 0;
int quit = 0;

char *get_time()
{
    time_t timer;
    static char buffer[26];
    struct tm *tm_info;
    timer = time(NULL);
    tm_info = localtime(&timer);
    strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    return buffer;
}

void print_exec_cmd(command_t *cmd, pid_t pid)
{
    fprintf(cmd->log_output, "%s - %s", get_time(), cmd->file);
    for (int i = 0; i < cmd->argc; i++)
    {
        fprintf(cmd->log_output, " %s", cmd->argv[i]);
    }
    fprintf(cmd->log_output, " has been executed with pid %d", pid);
    fprintf(cmd->log_output, "\n");
}

void print_proc_list(process_t *proc)
{
    process_t *current = proc;
    while (current != NULL)
    {
        printf("pid: %d --> ", current->pid);
        current = current->next;
    }
    printf("\n");
}

void kill_all_procs(process_t *proc)
{
    process_t *current = proc;
    process_t *tmp;
    while (current != NULL)
    {
        kill(current->pid, SIGKILL);
        tmp = current;
        current = current->next;
        free(tmp);
    }
}

process_t *add_process(pid_t pid)
{
    process_t *new_process;
    new_process = (process_t *)malloc(sizeof(process_t));
    if (!new_process)
    {
        fprintf(stderr, "add_process: out of memory\n");
        exit(EXIT_FAILURE);
    }
    new_process->pid = pid;
    strcpy(new_process->time, get_time());
    new_process->next = NULL;

    pthread_mutex_lock(&process_mutex);

    if (num_processes == 0)
    {
        processes = new_process;
        last_process = new_process;
    }
    else
    {
        last_process->next = new_process;
        last_process = new_process;
    }
    num_processes++;
    pthread_mutex_unlock(&process_mutex);
    return new_process;
}

void print_exec_cmd_attempt(command_t *cmd)
{
    fprintf(cmd->log_output, "%s - attempting to execute %s", get_time(), cmd->file);
    for (int i = 0; i < cmd->argc; i++)
    {
        fprintf(cmd->log_output, " %s", cmd->argv[i]);
    }
    fprintf(cmd->log_output, "\n");
}

void print_exec_cmd_err(command_t *cmd)
{
    fprintf(cmd->log_output, "%s - could not execute %s", get_time(), cmd->file);
    for (int i = 0; i < cmd->argc; i++)
    {
        fprintf(cmd->log_output, " %s", cmd->argv[i]);
    }
    fprintf(cmd->log_output, "\n");
}

void print_exec_cmd_exit(command_t *cmd, pid_t pid, int status)
{
    fprintf(cmd->log_output, "%s - %d has terminated with the exit status code %d\n", get_time(), pid, status);
}

void print_exec_cmd_sent_signal(command_t *cmd, pid_t pid, int sig)
{
    if (sig == SIGTERM)
    {
        fprintf(cmd->log_output, "%s - sent SIGTERM to %d\n", get_time(), pid);
    }
    else if (sig == SIGKILL)
    {
        fprintf(cmd->log_output, "%s - sent SIGKILL to %d\n", get_time(), pid);
    }
}

pid_t run_process(command_t *cmd)
{
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

    pid_t pid = fork();
    if (pid < 0)
    {
        fprintf(stderr, "fork() failed\n");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0)
    {
        if (redirect)
        {
            dup2(fileno(out_file), STDOUT_FILENO);
            dup2(fileno(out_file), STDERR_FILENO);
        }

        char *arg_arr[cmd->argc + 1];
        // First argument is file path
        arg_arr[0] = cmd->file;
        for (int i = 1; i < cmd->argc + 1; i++)
        {
            arg_arr[i] = cmd->argv[i - 1];
        }
        // char array must be null terminated
        arg_arr[cmd->argc + 1] = NULL;

        if (execv(cmd->file, arg_arr) == -1)
        {
            print_exec_cmd_err(cmd);
        }
    }
    else
    {
        return pid;
    }
    return getpid();
}

void exec_cmd(command_t *cmd)
{
    int status;
    int result;
    int term_pipe[2];

    struct timeval timeout;
    if (cmd->time[0] == '\0')
        timeout.tv_sec = 10;
    else
        timeout.tv_sec = atoi(cmd->time);
    timeout.tv_usec = 0;

    pipe(term_pipe);

    print_exec_cmd_attempt(cmd);
    pid_t ret = run_process(cmd);
    print_exec_cmd(cmd, ret);

    add_process(ret);

    close(term_pipe[1]);

    fd_set read_fds;

    FD_ZERO(&read_fds);
    FD_SET(term_pipe[0], &read_fds);
    result = select(term_pipe[0] + 1, &read_fds, NULL, NULL, &timeout);

    bool ended_manually = false;

    // Has timed out, must end manually
    if (result == 0)
    {

        kill(ret, SIGTERM);
        print_exec_cmd_sent_signal(cmd, ret, SIGTERM);

        int wait = 5000;

        // a current time of milliseconds
        int start = clock() * 1000 / CLOCKS_PER_SEC;

        // needed count milliseconds of return from this timeout
        int end = start + wait;
        do
        {
            start = clock() * 1000 / CLOCKS_PER_SEC;
        } while (kill(ret, 0) == 0 && start <= end);

        // If the process is still running 5 seconds after sending SIGTERM, must kill
        if (waitpid(ret, &status, WNOHANG) == 0)
        {
            kill(ret, SIGKILL);
            print_exec_cmd_sent_signal(cmd, ret, SIGKILL);
        }
        ended_manually = true;
    }

    close(term_pipe[0]);

    if (!ended_manually)
    {
        if (waitpid(ret, &status, 0) == -1)
        {
            perror("waitpid");
        }
    }

    if (WIFEXITED(status) || WIFSIGNALED(status))
    {
        int es = WEXITSTATUS(status);
        print_exec_cmd_exit(cmd, ret, es);
    }
}

void add_request(void (*func)(void *), void *data)
{
    struct request *a_request;
    a_request = (struct request *)malloc(sizeof(struct request));

    if (!a_request)
    {
        fprintf(stderr, "add_request: out of memory\n");
        exit(EXIT_FAILURE);
    }
    a_request->func = func;
    a_request->data = data;
    a_request->next = NULL;

    pthread_mutex_lock(&request_mutex);

    if (num_requests == 0)
    {
        requests = a_request;
        last_request = a_request;
    }
    else
    {
        last_request->next = a_request;
        last_request = a_request;
    }

    num_requests++;
    pthread_mutex_unlock(&request_mutex);
    pthread_cond_signal(&got_request);
}

struct request *get_request()
{
    struct request *a_request;
    if (num_requests > 0)
    {
        a_request = requests;
        requests = a_request->next;
        if (requests == NULL)
        {
            last_request = NULL;
        }
        num_requests--;
    }
    else
    {
        a_request = NULL;
    }

    return a_request;
}

void handle_request(struct request *a_request)
{
    a_request->func(a_request->data);
}

void *handle_requests_loop()
{
    struct request *a_request; /* pointer to a request.               */
    // int thread_id = *((int *)data); /* thread identifying number           */

    /* lock the mutex, to access the requests list exclusively. */
    pthread_mutex_lock(&request_mutex);

    /* while still running.... */
    int running = 1;
    while (running)
    {
        if (num_requests > 0)
        { /* a request is pending */
            a_request = get_request();
            if (a_request)
            {
                pthread_mutex_unlock(&request_mutex);
                handle_request(a_request);
                free(a_request);
                pthread_mutex_lock(&request_mutex);
            }
        }
        else
        {
            pthread_cond_wait(&got_request, &request_mutex);
        }

        pthread_mutex_lock(&quit_mutex);
        if (quit)
            running = 0;
        pthread_mutex_unlock(&quit_mutex);
    }
    pthread_mutex_unlock(&request_mutex);
    return NULL;
}

void recv_cmd_field(int conn_sockfd, char **field)
{
    int recv_len;
    if (recv(conn_sockfd, &recv_len, sizeof(int), PF_UNSPEC) == -1)
    {
        perror("recv");
        exit(EXIT_FAILURE);
    }
    int len = ntohs(recv_len);

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

    // Receive and execute command
    if (msg == cmd_msg_t)
    {
        int recv_val;
        char *file, *log, *out, *time;
        command_t *cmd = malloc(sizeof(command_t));
        cmd->argc = 0;

        recv_cmd_field(*conn_sockfd, &file);
        cmd->file = strdup(file);
        if (file)
            free(file);

        recv_cmd_field(*conn_sockfd, &log);
        cmd->log = strdup(log);
        if (log)
            free(log);

        recv_cmd_field(*conn_sockfd, &out);
        cmd->out = strdup(out);
        if (out)
            free(out);

        recv_cmd_field(*conn_sockfd, &time);
        cmd->time = strdup(time);
        if (time)
            free(time);

        if (recv(*conn_sockfd, &recv_val, sizeof(int), PF_UNSPEC) == -1)
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
                recv_cmd_field(*conn_sockfd, &cmd->argv[i]);
            }
        }

        close_sock(*conn_sockfd);

        if (cmd->log[0] == '\0')
        {
            cmd->log_output = stdout;
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
            cmd->log_output = logfile;
        }

        exec_cmd(cmd);

        // If process was not printing to stdout close the logfile
        if (cmd->log_output != stdout)
        {
            fclose(cmd->log_output);
        }

        free_cmd(cmd);
    }
}

void interrupt_handler()
{
    kill_all_procs(processes);
    close_sock(sockfd);
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
    int i;
    int thr_id[NUM_HANDLER_THREADS];
    pthread_t p_threads[NUM_HANDLER_THREADS];
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

    pthread_mutex_init(&request_mutex, NULL);
    pthread_mutex_init(&quit_mutex, NULL);
    pthread_mutex_init(&process_mutex, NULL);
    pthread_cond_init(&got_request, NULL);

    for (i = 0; i < NUM_HANDLER_THREADS; i++)
    {
        thr_id[i] = i;
        pthread_create(&p_threads[i], NULL, handle_requests_loop, &thr_id[i]);
    }

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

        printf("%s - connection received from %s\n", get_time(), inet_ntoa(their_addr.sin_addr));

        add_request(handle_conn, fd);
    }
}