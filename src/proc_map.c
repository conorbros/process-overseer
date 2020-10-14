#include "proc_map.h"

/**
 * @brief Parses a line from a proc file and extracts the required information
 * 
 * @param line 
 * @return proc_map_entry_t* 
 */
proc_map_entry_t *parse_proc_maps_line(char *line)
{
    proc_map_entry_t *proc_map_entry = malloc(sizeof(*proc_map_entry));
    char *str = line;
    proc_map_entry->addr_start = strtoull(strtok_r(str, "-", &str), NULL, 16);
    proc_map_entry->addr_end = strtoull(strtok_r(str, " ", &str), NULL, 16);

    for (int i = 0; i < 3; i++)
    {
        strtok_r(str, " ", &str);
    }
    proc_map_entry->inode = atoi(strtok_r(str, " ", &str));

    return proc_map_entry;
}

/**
 * @brief Expands a proc map array if the default capacity of 32 is filled
 * 
 * @param array 
 * @param n 
 */
void expand_array(proc_maps_entries **array, int n)
{
    array = realloc(array, (n + 16) * sizeof(*array));
    memset(array + n, 0, 16 * sizeof(*array));
    n += 16;
}

/**
 * @brief Builds the proc map entry array from an opened proc map files
 * 
 * @param file 
 * @return proc_maps_entries** 
 */
proc_maps_entries **build_entry_array(FILE *file)
{
    size_t i = 0, size = 0, n = DEFAULT_ARRAY_SIZE;
    proc_maps_entries **entries = calloc(n, sizeof(*entries));
    char *line = NULL;

    while (getline(&line, &size, file) != -1)
    {
        if (line != NULL)
        {
            entries[i] = parse_proc_maps_line(line);
        }

        i++;

        if (i == n)
        {
            expand_array(entries, n);
        }

        free(line);
        line = NULL;
    }

    free(line);

    return entries;
}

/**
 * @brief Get an array of the proc map entries for the specified pid
 * 
 * @param pid 
 * @return proc_maps_entries** 
 */
proc_maps_entries **get_proc_maps(pid_t pid)
{
    char path[DEFAULT_PATH_SIZE];
    FILE *file = NULL;
    // int errno_saver = 0;
    proc_maps_entries **entries = NULL;

    sprintf(path, "/proc/%d/maps", pid);

    file = fopen(path, "r");
    if (file == NULL)
    {
        perror("fopen");
        return NULL;
    }
    entries = build_entry_array(file);
    fclose(file);
    return entries;
}

/**
 * @brief Frees an array of proc map entries
 * 
 * @param entries 
 */
void free_procmaps(proc_maps_entries **entries)
{
    int i = 0;
    while (entries[i] != NULL)
    {
        free(entries[i]);
        i++;
    }
    free(entries);
}

/**
 * @brief Returns the length of the address specified by start and end
 * 
 * @param start address beginning 
 * @param end address end
 * @return unsigned long length in bytes of the address
 */
unsigned long addr_len(unsigned long long start, unsigned long long end)
{
    return end - start;
}

/**
 * @brief Get the memory that the process specified by the supplied pid is currently using in bytes
 * 
 * @param pid 
 * @return int 
 */
int get_bytes_proc_using(pid_t pid)
{
    proc_maps_entries **results = get_proc_maps(pid);
    unsigned long long bytes = 0;
    int i = 0;
    while (results[i] != NULL)
    {
        if (results[i]->inode == 0)
        {
            bytes += addr_len(results[i]->addr_start, results[i]->addr_end);
        }
        i++;
    }
    free_procmaps(results);
    return (int)bytes;
}