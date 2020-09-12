#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define WAIT 1

int main(int argc, char *argv[])
{
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