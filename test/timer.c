#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define WAIT 1

int main(int argc, char *argv[])
{
    printf("Starting the timer\n");
    if (argc < 2)
    {
        printf("Waiting for %d\n", WAIT);
        sleep(WAIT);
    }
    else
    {
        int wait = atoi(argv[1]);
        printf("Waiting for %d\n", wait);
        sleep(wait);
    }

    printf("Finished\n");
    return 0;
}