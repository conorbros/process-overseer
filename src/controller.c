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

#define MAXDATASIZE 100 /* max number of bytes we can get at once */

#define ARRAY_SIZE 30

#define RETURNED_ERROR -1

void print_usage()
{
    fprintf(stderr, "Usage: controller <address> <port> {[-o out_file] [-log log_file] [-t seconds] <file> [arg...] | mem [pid] | memkill <percent>}\n");
}

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

void get_cmd_args(int argc, char *argv[], command_t *cmd)
{
    int j = 0;
    for (int i = 3; i < argc; i += 2)
    {

        if (strcmp(argv[i], "-o") == 0)
        {
            cmd->out = argv[i + 1];
        }
        else if (strcmp(argv[i], "-t") == 0)
        {
            if (!is_str_number(argv[i + 1]))
            {
                print_usage();
                exit(1);
            }
            cmd->time = argv[i + 1];
        }
        else if (strcmp(argv[i], "-log") == 0)
        {
            cmd->log = argv[i + 1];
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
}

void send_cmd_field(int sockfd, char *field)
{
    const int len = strlen(field) + 1;
    const int val = htons(len);
    if (send(sockfd, &val, sizeof(int), PF_UNSPEC) == -1)
    {
        perror("send");
        exit(EXIT_FAILURE);
    }

    if (send(sockfd, field, len, PF_UNSPEC) == -1)
    {
        perror("send");
        exit(EXIT_FAILURE);
    }
}

void send_cmd_arg(int sockfd, char *arg)
{
    const int len = strlen(arg) + 1;
    const int val = htons(len);
    if (send(sockfd, &val, sizeof(int), PF_UNSPEC) == -1)
    {
        perror("send");
        exit(EXIT_FAILURE);
    }
    if (send(sockfd, arg, len, PF_UNSPEC) == -1)
    {
        perror("send");
        exit(EXIT_FAILURE);
    }
}

void send_cmd_args(int sockfd, command_t *cmd)
{
    const int val = htons(cmd->argc);
    if (send(sockfd, &val, sizeof(int), PF_UNSPEC) == -1)
    {
        perror("send");
        exit(1);
    }
    for (int i = 0; i < cmd->argc; i++)
    {
        send_cmd_arg(sockfd, cmd->argv[i]);
    }
}

void free_cmd(command_t *cmd)
{
    free(cmd->argv);
    free(cmd);
}

int main(int argc, char *argv[])
{
    int sockfd;
    struct hostent *he;
    struct sockaddr_in their_addr; /* connector's address information */
    char *hostname;
    int port;

    if (argc < 3)
    {
        print_usage();
        exit(EXIT_FAILURE);
    }

    if (strcmp(argv[1], "--help") == 0)
    {
        print_usage();
        exit(EXIT_SUCCESS);
    }

    command_t *cmd = malloc(sizeof(command_t));
    get_cmd_args(argc, argv, cmd);

    // First 2 arguments must be host and port
    hostname = argv[1];
    if (!is_str_number(argv[2]))
    {
        print_usage();
        exit(EXIT_FAILURE);
    }
    port = atoi(argv[2]);

    if ((he = gethostbyname(hostname)) == NULL)
    { /* get the host info */
        herror("gethostbyname");
        exit(1);
    }

    if ((sockfd = socket(AF_INET, SOCK_STREAM, PF_UNSPEC)) == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /* clear address struct */
    memset(&their_addr, 0, sizeof(their_addr));

    their_addr.sin_family = AF_INET;   /* host byte order */
    their_addr.sin_port = htons(port); /* short, network byte order */
    their_addr.sin_addr = *((struct in_addr *)he->h_addr);

    if (connect(sockfd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr)) == -1)
    {
        fprintf(stderr, "Could not connect to overseer at %s %d\n", inet_ntoa(their_addr.sin_addr), atoi(argv[2]));
        exit(1);
    }

    // Send message type
    const int msg_type = htons(cmd_msg_t);
    if (send(sockfd, &msg_type, sizeof(int), PF_UNSPEC) == -1)
    {
        perror("send");
        exit(EXIT_FAILURE);
    }

    send_cmd_field(sockfd, cmd->file);

    if (cmd->log == NULL)
    {
        cmd->log = "";
    }
    send_cmd_field(sockfd, cmd->log);

    if (cmd->out == NULL)
    {
        cmd->out = "";
    }
    send_cmd_field(sockfd, cmd->out);

    if (cmd->time == NULL)
    {
        cmd->time = "";
    }
    send_cmd_field(sockfd, cmd->time);

    send_cmd_args(sockfd, cmd);

    close(sockfd);

    free_cmd(cmd);

    return 0;
}