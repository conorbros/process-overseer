#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#define WAIT 1

void termination_handler()
{
    printf("Received termination signal...exiting\n");
    sleep(3);
    exit(SIGTERM);
}

int main(int argc, char *argv[])
{
    signal(SIGTERM, termination_handler);
    for (int i = 1; i < argc; i++)
    {
        printf("%s ", argv[i]);
    }
    printf("\n");

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
    return 2;
}
