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

pthread_mutex_t request_mutex;
pthread_mutex_t quit_mutex;

pthread_mutex_t process_mutex;

pthread_cond_t got_request;

int num_requests = 0;
int quit = 0;

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

process_t *processes = NULL;

struct request *requests = NULL;
struct request *last_request = NULL;

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

void print_exec_cmd_log(command_t *cmd)
{
    char *output;
    int len = cmd->file;
    for (int i = 0; i < cmd->argc; i++)
    {
        len += strlen(cmd->argv[i]) + 2;
    }
    output = (char *)malloc(len);
    sprintf(output, "%s - attempting to execute %s", get_time(), cmd->file);
    printf("%s", output);
    for (int i = 0; i < cmd->argc; i++)
    {
        printf(" %s", cmd->argv[i]);
    }
    printf("\n");
    free(output);
}

void exec_cmd(command_t *cmd)
{
    print_exec_cmd_log(cmd);
}

void add_request(void (*func)(void *), void *data)
{
    struct request *a_request;
    a_request = (struct request *)malloc(sizeof(struct request));

    if (!a_request)
    {
        fprintf(stderr, "add_request: out of memory\n");
        exit(1);
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

void recv_cmd_field(int sockfd, char **field)
{
    int recv_len;
    if (recv(sockfd, &recv_len, sizeof(int), PF_UNSPEC) == -1)
    {
        perror("recv");
        exit(1);
    }
    int len = ntohs(recv_len);

    char recv_char[len];
    if (recv(sockfd, &recv_char, sizeof(char) * len, PF_UNSPEC) == -1)
    {
        perror("recv");
        exit(1);
    }
    *field = (char *)malloc(len);
    memcpy(*field, recv_char, len);
}

void close_sock(int sockfd)
{
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
}

void handle_conn(void *arg)
{
    int val;
    int *sockfd = arg;
    if (recv(*sockfd, &val, sizeof(int), PF_UNSPEC) == -1)
    {
        perror("recv");
        exit(1);
    }
    msg_t msg = ntohs(val);

    // Receive command struct
    if (msg == cmd_msg_t)
    {
        int recv_val;
        char *file, *log, *out, *time;
        command_t *cmd = (command_t *)malloc(sizeof(command_t *));
        cmd->argc = 0;

        recv_cmd_field(*sockfd, &file);
        cmd->file = strdup(file);
        free(file);

        recv_cmd_field(*sockfd, &log);
        cmd->log = strdup(log);
        if (*log)
            free(log);

        recv_cmd_field(*sockfd, &out);
        cmd->out = strdup(out);
        if (*out)
            free(out);

        recv_cmd_field(*sockfd, &time);
        cmd->time = strdup(time);
        if (*time)
            free(time);

        if (recv(*sockfd, &recv_val, sizeof(int), PF_UNSPEC) == -1)
        {
            perror("recv");
            exit(1);
        }

        cmd->argc = ntohs(recv_val);

        cmd->argv = malloc(sizeof(char *) * cmd->argc);
        // Receive command args
        if (cmd->argc > 0)
        {
            for (int i = 0; i < cmd->argc; i++)
            {
                recv_cmd_field(*sockfd, &cmd->argv[i]);
            }
        }

        close_sock(*sockfd);

        exec_cmd(cmd);
    }
}

int main(int argc, char *argv[])
{
    int i;
    int thr_id[NUM_HANDLER_THREADS];
    pthread_t p_threads[NUM_HANDLER_THREADS];
    int sockfd;
    int new_fd;
    struct sockaddr_in my_addr;
    struct sockaddr_in their_addr;
    socklen_t sin_size;
    int port;

    if (argc < 2)
    {
        fprintf(stderr, "Usage: <port>\n");
        return 1;
    }
    port = atoi(argv[1]);

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        exit(1);
    }

    pthread_mutex_init(&request_mutex, NULL);
    pthread_mutex_init(&quit_mutex, NULL);
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
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1)
    {
        perror("listen");
        exit(1);
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
        // shutdown(new_fd, SHUT_RDWR);
        // close(new_fd);
    }
}