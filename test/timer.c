#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#define WAIT 1

int term_wait;

void termination_handler()
{
    printf("Received termination signal...exiting\n");
    sleep(term_wait);
    exit(SIGTERM);
}

int main(int argc, char *argv[])
{
    term_wait = atoi(argv[2]);
    signal(SIGTERM, termination_handler);
    if (argc < 1)
    {
        printf("Waiting for  default: %d\n", WAIT);
        sleep(WAIT);
    }
    else
    {
        printf("Starting the timer\n");
        int wait = atoi(argv[1]);
        printf("Waiting for custom: %d\n", wait);
        sleep(wait);
    }

    printf("Finished\n");
    return 0;
}