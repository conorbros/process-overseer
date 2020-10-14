#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

#define WAIT 1

void termination_handler()
{
    printf("Received termination signal...exiting\n");
    sleep(10);
    exit(SIGTERM);
}

int main(int argc, char *argv[])
{
    signal(SIGTERM, termination_handler);
    // int mb = 1e+6 * atoi(argv[2]);
    // int *big = malloc(mb);
    if (argc < 1)
    {
        printf("Waiting for  default: %d\n", WAIT);
        sleep(WAIT);
    }
    else
    {

        // printf("Starting the timer\n");
        // int wait =
        //     printf("Waiting for custom: %d\n", wait);
        // sleep(wait);
        for (int i = 0; i < atoi(argv[1]); i++)
        {
            sleep(1);
            malloc(1e+6);
        }
    }

    printf("Finished\n");
    // free(big);
    return 0;
}