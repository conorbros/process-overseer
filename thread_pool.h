#ifndef H_THREAD_POOL
#define H_THREAD_POOL

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define NUM_HANDLER_THREADS 5

struct request
{
    void (*func)(void *);
    void *data;
    struct request *next;
};

void add_request(void (*func)(void *), void *data);

struct request *get_request();

void handle_request(struct request *a_request);

void *handle_requests_loop();

void init_threadpool();

void quit_threads();

#endif