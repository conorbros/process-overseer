#include <stdio.h>
#include <stdlib.h>

int main()
{
    unsigned long total = 100;
    int bytes = 40;

    float result = (float)bytes / (float)total;

    printf("%4.1f\n", result);

    return 0;
}