#include "thread_pool.h"

pthread_mutex_t request_mutex;
pthread_mutex_t quit_mutex;
pthread_cond_t got_request;

struct request *requests = NULL;
struct request *last_request = NULL;

int num_requests = 0;
int quit = 0;

int thr_id[NUM_HANDLER_THREADS];
pthread_t p_threads[NUM_HANDLER_THREADS];

/**
 * @brief Adds a request to the queue to be handled by the next available thread
 * 
 * @param func 
 * @param data 
 */
void add_request(void (*func)(void *), void *data)
{
    struct request *a_request;
    a_request = (struct request *)malloc(sizeof(struct request));

    if (!a_request)
    {
        fprintf(stderr, "add_request: out of memory\n");
        exit(EXIT_FAILURE);
    }
    a_request->func = func;
    a_request->data = data;
    a_request->next = NULL;

    pthread_mutex_lock(&request_mutex);

    if (num_requests == 0)
    {
        requests = a_request;
        last_request = a_request;
    }
    else
    {
        last_request->next = a_request;
        last_request = a_request;
    }

    num_requests++;
    pthread_mutex_unlock(&request_mutex);
    pthread_cond_signal(&got_request);
}

/**
 * @brief Get the first request from the queue
 * 
 * @return struct request* 
 */
struct request *get_request()
{
    struct request *a_request;
    if (num_requests > 0)
    {
        a_request = requests;
        requests = a_request->next;
        if (requests == NULL)
        {
            last_request = NULL;
        }
        num_requests--;
    }
    else
    {
        a_request = NULL;
    }

    return a_request;
}

void handle_request(struct request *a_request)
{
    a_request->func(a_request->data);
}

/**
 * @brief Function for the threads to run, grabbing requests from the queue and handling them
 * 
 * @return void* 
 */
void *handle_requests_loop()
{
    struct request *a_request;

    pthread_mutex_lock(&request_mutex);

    int running = 1;
    while (running)
    {
        if (num_requests > 0)
        {
            a_request = get_request();
            if (a_request)
            {
                pthread_mutex_unlock(&request_mutex);
                handle_request(a_request);
                free(a_request);
                pthread_mutex_lock(&request_mutex);
            }
        }
        else
        {
            pthread_cond_wait(&got_request, &request_mutex);
        }

        pthread_mutex_lock(&quit_mutex);
        if (quit)
        {
            running = 0;
        }

        pthread_mutex_unlock(&quit_mutex);
    }
    pthread_mutex_unlock(&request_mutex);
    return NULL;
}

/**
 * @brief Initializes the threadpool and mutexes
 * 
 */
void init_threadpool()
{
    pthread_mutex_init(&request_mutex, NULL);
    pthread_mutex_init(&quit_mutex, NULL);
    pthread_cond_init(&got_request, NULL);

    for (int i = 0; i < NUM_HANDLER_THREADS; i++)
    {
        thr_id[i] = i;
        pthread_create(&p_threads[i], NULL, handle_requests_loop, &thr_id[i]);
    }
}

/**
 * @brief Quits and joins the threads, called on SIGINT
 * 
 */
void quit_threads()
{
    pthread_mutex_lock(&quit_mutex);
    quit = 1;
    pthread_mutex_unlock(&quit_mutex);
    pthread_cond_broadcast(&got_request);

    for (int i = 0; i < NUM_HANDLER_THREADS; i++)
    {
        pthread_join(p_threads[i], NULL);
    }
}