#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
// #define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

#define USEC_PER_MSEC (1000)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    if(thread_param == NULL){
        DEBUG_LOG("Cannot pass null args to threadfunc. Returning.");
        return NULL;
    }
    thread_data_t* args = (thread_data_t*) thread_param;
    int rc = 0;

    DEBUG_LOG("Starting wait sleep...\n");
    usleep(args->wait_to_obtain_ms * USEC_PER_MSEC);

    DEBUG_LOG("Locking Mutex\n");
    rc = pthread_mutex_lock(args->mutex);
    if(rc != 0){
        perror("pthread_mutex_lock");
    }


    DEBUG_LOG("Starting wait to release\n");
    usleep(args->wait_to_release_ms * USEC_PER_MSEC);

    DEBUG_LOG("Releasing Mutex\n");
    pthread_mutex_unlock(args->mutex);

    args->thread_complete_success = true;



    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */

    thread_data_t* threadDataPtr = malloc(sizeof(thread_data_t));
    if (threadDataPtr == NULL){
        DEBUG_LOG("threadData could not be mallocd. Exiting");
        return false;
    }

    threadDataPtr->mutex = mutex;
    threadDataPtr->wait_to_obtain_ms = wait_to_obtain_ms;
    threadDataPtr->wait_to_release_ms = wait_to_release_ms;

    int rc = pthread_create(thread, NULL, threadfunc, threadDataPtr);
    if(rc != 0){
        perror("pthread_create: ");
        return false;
    }

    // pthread_join(*thread, NULL);

    // free(threadDataPtr);



    return true;
}

