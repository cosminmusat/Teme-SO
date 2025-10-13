#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "types.h"

static scheduler* sch = NULL;
static pthread_mutex_t sch_mutex;
static pthread_cond_t all_threads_terminated;

DECL_PREFIX int so_init(unsigned int time_quantum, unsigned int io) {
    if (time_quantum == 0) {
        return -1;
    }

    if (io > SO_MAX_NUM_EVENTS) {
        return -1;
    }

    if (sch != NULL) {
        return -1;
    }

    sch = calloc(1, sizeof(*sch));

    if (sch == NULL) {
        return -1;
    }

    sch->time_quantum = time_quantum;
    sch->io = io;
    sch->no_threads = 0;
    sch->no_terminated_threads = 0;
    pthread_mutex_init(&sch_mutex, NULL);
    pthread_cond_init(&all_threads_terminated, NULL);

    return 0;
}

void* thread_function(void* arg) {
    thread* tr = (thread*)arg;
    unsigned int priority = tr->priority;
    fprintf(stderr, "Thread with priority %u started\n", priority);
    tr->tid = pthread_self();
    tr->func(priority);

    pthread_mutex_lock(&sch_mutex);
    sch->terminated_threads[sch->no_terminated_threads++] = tr;
    pthread_cond_signal(&all_threads_terminated);
    pthread_mutex_unlock(&sch_mutex);

    return NULL;
}

DECL_PREFIX tid_t so_fork(so_handler *func, unsigned int priority) {
    if (func == 0) {
        return INVALID_TID;
    }

    if (priority > SO_MAX_PRIO) {
        return INVALID_TID;
    }

    // fprintf(stderr, "Forking thread with priority %u\n", priority);

    thread* tr = malloc(sizeof(thread));
    tr->time_quantum = sch->time_quantum;
    tr->priority = priority;
    tr->func = func;

    pthread_t ptr;
    pthread_mutex_lock(&sch_mutex);
    sch->no_threads++;
    pthread_mutex_unlock(&sch_mutex);

    pthread_create(&ptr, NULL, thread_function, tr);

    return ptr;
}

DECL_PREFIX int so_wait(unsigned int io) {
    return 0;
}

DECL_PREFIX int so_signal(unsigned int io) {
    return 0;
}

DECL_PREFIX void so_exec(void) {
    return;
}

DECL_PREFIX void so_end(void) {
    if (sch == NULL) {
        return;
    }
    
    pthread_mutex_lock(&sch_mutex);
    while (sch->no_threads - sch->no_terminated_threads > 0) {
        pthread_cond_wait(&all_threads_terminated, &sch_mutex);
    }
    pthread_mutex_unlock(&sch_mutex);

    for (unsigned int i = 0; i < sch->no_threads; i++) {
        pthread_join(sch->terminated_threads[i]->tid, NULL);
        free(sch->terminated_threads[i]);
    }

    free(sch);
    sch = NULL;

    pthread_mutex_destroy(&sch_mutex);
    pthread_cond_destroy(&all_threads_terminated);

    return;
}