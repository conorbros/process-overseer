#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <stdbool.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ctype.h>
#include "comm.h"

#define ARRAY_SIZE 30

#define ERROR_T -1

typedef struct sock
{
    int fd;
    struct sockaddr_in *their_addr;
    in_port_t port;
} sock_t;

sock_t *overseer_sock;

void print_usage(bool err)
{
    FILE *out;
    if (err)
        out = stderr;
    else
        out = stdout;

    fprintf(out, "Usage: controller <address> <port> {[-o out_file] [-log log_file] [-t seconds] <file> [arg...] | mem [pid] | memkill <percent>}\n");
}

void print_could_not_connect(char *hostname, in_port_t port)
{
    fprintf(stderr, "Could not connect to overseer at %s %d\n", hostname, port);
}

void connect_to_overseer()
{
    if (connect(overseer_sock->fd, (struct sockaddr *)overseer_sock->their_addr, sizeof(struct sockaddr)) == ERROR_T)
    {
        print_could_not_connect(inet_ntoa(overseer_sock->their_addr->sin_addr), overseer_sock->port);
        exit(1);
    }
}

void free_cmd(command_t *cmd)
{
    free(cmd->argv);
    free(cmd);
}

void free_sock_t(sock_t *sock)
{
    free(sock->their_addr);
    free(sock);
}

/**
 * @brief Determines if the supplied string contains digits only
 * 
 * @param string 
 * @return true 
 * @return false 
 */
bool is_str_number(const char *string)
{
    const int len = strlen(string);
    for (int i = 0; i < len; ++i)
    {
        if (!isdigit(string[i]))
            return false;
    }
    return true;
}

/**
 * @brief Gets the command arguments from argv
 * 
 * @param argc 
 * @param argv 
 * @param cmd command_t to attach arguments to
 */
void get_cmd_args(int argc, char *argv[], command_t *cmd)
{
    int j = 0;
    bool opts[3];
    for (int i = 0; i < 3; i++)
    {
        opts[i] = false;
    }

    for (int i = 3; i < argc; i += 2)
    {
        if (strcmp(argv[i], "-o") == 0)
        {
            if (opts[0])
            {
                print_usage(true);
                exit(EXIT_FAILURE);
            }

            cmd->out = argv[i + 1];
            opts[0] = true;
        }
        else if (strcmp(argv[i], "-log") == 0)
        {
            if (opts[1])
            {
                print_usage(true);
                exit(EXIT_FAILURE);
            }
            cmd->log = argv[i + 1];
            opts[0] = true;
            opts[1] = true;
        }
        else if (strcmp(argv[i], "-t") == 0)
        {
            if (!is_str_number(argv[i + 1]) || opts[2])
            {
                print_usage(true);
                exit(EXIT_FAILURE);
            }
            cmd->time = argv[i + 1];
            opts[0] = true;
            opts[1] = true;
            opts[2] = true;
        }
        else // must be the file
        {
            cmd->file = argv[i];

            i++;

            cmd->argc = argc - i;
            cmd->argv = malloc(sizeof(char *) * cmd->argc);
            while (i < argc)
            {
                cmd->argv[j] = argv[i];
                j++;
                i++;
            }
        }
    }

    if (cmd->file == NULL)
    {
        free_cmd(cmd);
        print_usage(true);
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief Sends a command struct member to the overseer socket
 * 
 * @param field 
 */
void send_cmd_field(const char *field)
{
    const int len = strlen(field) + 1;
    const int val = htons(len);
    if (send(overseer_sock->fd, &val, sizeof(uint32_t), PF_UNSPEC) == -1)
    {
        perror("send");
        exit(EXIT_FAILURE);
    }

    if (send(overseer_sock->fd, field, sizeof(char) * len, PF_UNSPEC) == -1)
    {
        perror("send");
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief Sends a single command argument to the overseer socket
 * 
 * @param arg 
 */
void send_cmd_arg(const char *arg)
{
    const int len = strlen(arg) + 1;
    const int val = htons(len);
    if (send(overseer_sock->fd, &val, sizeof(int), PF_UNSPEC) == -1)
    {
        perror("send");
        exit(EXIT_FAILURE);
    }
    if (send(overseer_sock->fd, arg, sizeof(char) * len, PF_UNSPEC) == -1)
    {
        perror("send");
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief Sends the command arguments to the overseer socket
 * 
 * @param cmd 
 */
void send_cmd_args(command_t *cmd)
{
    const int val = htons(cmd->argc);
    if (send(overseer_sock->fd, &val, sizeof(int), PF_UNSPEC) == -1)
    {
        perror("send");
        exit(1);
    }
    for (int i = 0; i < cmd->argc; i++)
    {
        send_cmd_arg(cmd->argv[i]);
    }
}

/**
 * @brief Send the message type to let the overseer know what message to expect
 * 
 * @param msg_type 
 */
void send_msg_type(msg_t msg_type)
{
    const int msg_type_ns = htons(msg_type);
    if (send(overseer_sock->fd, &msg_type_ns, sizeof(int), PF_UNSPEC) == ERROR_T)
    {
        perror("send");
        exit(EXIT_FAILURE);
    }
}

command_t *create_cmd()
{
    command_t *cmd = malloc(sizeof(command_t));

    cmd->file = NULL;
    cmd->log = NULL;
    cmd->out = NULL;
    cmd->time = NULL;
    cmd->argv = NULL;
    return cmd;
}

/**
 * @brief Handles sending a command to the overseer
 * 
 * @param argc 
 * @param argv 
 */
void handle_cmd(int argc, char **argv)
{

    command_t *cmd = create_cmd();
    get_cmd_args(argc, argv, cmd);

    connect_to_overseer();
    send_msg_type(cmd_msg_t);
    send_cmd_field(cmd->file);

    if (cmd->log == NULL)
    {
        cmd->log = "";
    }
    send_cmd_field(cmd->log);

    if (cmd->out == NULL)
    {
        cmd->out = "";
    }
    send_cmd_field(cmd->out);

    if (cmd->time == NULL)
    {
        cmd->time = "";
    }
    send_cmd_field(cmd->time);

    send_cmd_args(cmd);

    free_cmd(cmd);
}

/**
 * @brief Receives memory record entry from the overseer
 * 
 */
void recv_mem_entry()
{
    int recv_len;
    if (recv(overseer_sock->fd, &recv_len, sizeof(int), PF_UNSPEC) == -1)
    {
        perror("recv");
        exit(EXIT_FAILURE);
    }
    const int len = ntohs(recv_len);
    char recv_char[len];
    if (recv(overseer_sock->fd, recv_char, sizeof(char) * len, PF_UNSPEC) == -1)
    {
        perror("recv");
        exit(EXIT_FAILURE);
    }
    printf("%s\n", recv_char);
}

/**
 * @brief Receives the latest memory record entry for each running process from the overseer
 * 
 */
void handle_mem_all()
{
    connect_to_overseer();
    send_msg_type(mem_all_msg_t);

    int recv_count, len;
    if (recv(overseer_sock->fd, &recv_count, sizeof(int), PF_UNSPEC) == -1)
    {
        perror("recv");
        exit(EXIT_FAILURE);
    }
    len = ntohs(recv_count);

    for (int i = 0; i < len; i++)
    {
        recv_mem_entry(overseer_sock->fd);
    }
}

/**
 * @brief Receives all memory record entries for the process specified by the supplied pid from the overseer
 * 
 * @param argv 
 */
void handle_mem_pid(char **argv)
{

    if (!is_str_number(argv[4]))
    {
        print_usage(true);
        return;
    }

    connect_to_overseer();

    send_msg_type(mem_pid_msg_t);
    int val = htons(atoi(argv[4]));
    if (send(overseer_sock->fd, &val, sizeof(int), PF_UNSPEC) == -1)
    {
        perror("send");
        exit(EXIT_FAILURE);
    }

    int len;
    if (recv(overseer_sock->fd, &len, sizeof(int), PF_UNSPEC) == -1)
    {
        perror("recv");
        exit(EXIT_FAILURE);
    }
    len = ntohs(len);
    for (int i = 0; i < len; i++)
    {
        recv_mem_entry();
    }
}

/**
 * @brief Entry point for both types of mem commands
 * 
 * @param argc 
 * @param argv 
 */
void handle_mem(int argc, char **argv)
{
    if (argc < 5)
    {
        handle_mem_all();
    }
    else
    {
        handle_mem_pid(argv);
    }
}

bool is_int_or_float(char *buffer)
{
    int i, r;
    size_t n;
    r = sscanf(buffer, "%d%zu", &i, &n);
    if (r == 1 && n == strlen(buffer))
    {
        return true;
    }

    double dbl;
    r = sscanf(buffer, "%lf", &dbl);
    return r == 1;
}

/**
 * @brief Handles sending a memkill command to the overseer
 * 
 * @param argc 
 * @param argv 
 */
void handle_memkill(int argc, char **argv)
{
    if (argc < 5)
    {
        print_usage(true);
        return;
    }

    if (!is_int_or_float(argv[4]))
    {
        print_usage(true);
        return;
    }

    float mem_threshold_f = strtof(argv[4], NULL);
    if (mem_threshold_f > (float)100 || mem_threshold_f < (float)0)
    {
        print_usage(true);
        return;
    }

    connect_to_overseer();
    send_msg_type(memkill_msg_t);

    char *mem_threshold = argv[4];
    const int len = strlen(mem_threshold) + 1;
    int val = htons(len);
    if (send(overseer_sock->fd, &val, sizeof(int), PF_UNSPEC) == -1)
    {
        perror("send");
        exit(EXIT_FAILURE);
    }
    if (send(overseer_sock->fd, mem_threshold, sizeof(char) * len, PF_UNSPEC) == -1)
    {
        perror("send");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[])
{
    struct hostent *he;
    char *hostname;

    if (argc < 4)
    {
        print_usage(true);
        exit(EXIT_FAILURE);
    }

    if (strcmp(argv[1], "--help") == 0)
    {
        print_usage(false);
        exit(EXIT_SUCCESS);
    }

    overseer_sock = (sock_t *)malloc(sizeof(sock_t));

    // First 2 arguments must be host and port
    hostname = argv[1];
    if (!is_str_number(argv[2]))
    {
        print_usage(true);
        exit(EXIT_FAILURE);
    }
    overseer_sock->port = atoi(argv[2]);

    if ((he = gethostbyname(hostname)) == NULL)
    {
        print_could_not_connect(hostname, overseer_sock->port);
        exit(1);
    }

    if ((overseer_sock->fd = socket(AF_INET, SOCK_STREAM, PF_UNSPEC)) == ERROR_T)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    overseer_sock->their_addr = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
    memset(overseer_sock->their_addr, 0, sizeof(struct sockaddr_in));
    overseer_sock->their_addr->sin_family = AF_INET;
    overseer_sock->their_addr->sin_port = htons(overseer_sock->port);
    overseer_sock->their_addr->sin_addr = *((struct in_addr *)he->h_addr);

    if (strcmp(argv[3], "mem") == 0)
    {
        handle_mem(argc, argv);
    }
    else if (strcmp(argv[3], "memkill") == 0)
    {
        handle_memkill(argc, argv);
    }
    else
    {
        handle_cmd(argc, argv);
    }

    close(overseer_sock->fd);
    shutdown(overseer_sock->fd, SHUT_RDWR);
    free_sock_t(overseer_sock);

    return EXIT_SUCCESS;
}