#ifndef PROCMAPS_H_
#define PROCMAPS_H_

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>

#define DEFAULT_ARRAY_SIZE 32
#define DEFAULT_PATH_SIZE 100

typedef struct proc_map_entry
{
    unsigned long long addr_start;
    unsigned long long addr_end;
    int inode;
} proc_map_entry_t;

typedef proc_map_entry_t proc_maps_entries;

int get_bytes_proc_using(int pid);

#endif