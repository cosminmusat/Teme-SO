#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#ifndef TYPES
#include "types.h"
#endif

#ifndef CQUEUE
#include "circular_queue.h"
#endif

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
    sch->running_thread = NULL;

    for (int i = SO_MAX_PRIO; i >= 0; i--) {
        init_queue(&sch->ready_queues[i]);
    }

    pthread_mutex_init(&sch_mutex, NULL);
    pthread_cond_init(&all_threads_terminated, NULL);

    return 0;
}

void wait_to_run() {
    pthread_mutex_lock(&sch_mutex);
    while (sch->running_thread == NULL || pthread_self() != sch->running_thread->tid) {
        // fprintf(stderr, "Thread %lu waiting to get CPU\n", pthread_self());
        if (sch->running_thread != NULL) {
            fprintf(stderr, "Current running thread is %lu\n", sch->running_thread->tid);
        }
        pthread_mutex_unlock(&sch_mutex);
        sched_yield();
        pthread_mutex_lock(&sch_mutex);
    }
    pthread_mutex_unlock(&sch_mutex);
}

void terminate() {
    pthread_mutex_lock(&sch_mutex);
    sch->terminated_threads[sch->no_terminated_threads++] = tr;
    pthread_cond_signal(&all_threads_terminated);
}

void* thread_function(void* arg) {

    thread* tr = (thread*)arg;
    tr->tid = pthread_self();

    wait_to_run();
    
    unsigned int priority = tr->priority;
    tr->func(priority);

    

    sch->running_thread = NULL;

    pthread_mutex_unlock(&sch_mutex);

    tr->work_done = 1;

    return NULL;
}

void check_scheduler() {

    fprintf(stderr, "Running scheduler\n");

    pthread_mutex_lock(&sch_mutex);
    thread* curr = sch->running_thread;

    if (curr != NULL) {
        if (curr->work_done == 0) {
            enqueue(&sch->ready_queues[curr->priority], curr);
            curr = NULL;
        }
    }

    thread* next = NULL;
    for (int i = SO_MAX_PRIO; i >= 0; --i) {
        fprintf(stderr, "front %d and rear %d of pirority %d q\n", sch->ready_queues[i].front, sch->ready_queues[i].rear, i);
        C_Queue q = sch->ready_queues[i];
        thread* tr = dequeue(&q);
        if (tr != NULL) {
            next = tr;
            break;
        }
    }

    if (next == NULL) {
        fprintf(stderr, "Didnt find any thread to schedule\n");
        pthread_mutex_unlock(&sch_mutex);
        return;
    }
    
    fprintf(stderr, "Found thread to schedule\n");

    next->time_quantum = sch->time_quantum;
    sch->running_thread = next;

    if (curr != NULL) {
        enqueue(&sch->ready_queues[curr->priority], curr);
    }
    pthread_mutex_unlock(&sch_mutex);
    return;
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
    tr->work_done = 0;

    pthread_t ptr;
    pthread_mutex_lock(&sch_mutex);
    sch->no_threads++;
    pthread_mutex_unlock(&sch_mutex);

    pthread_create(&ptr, NULL, thread_function, tr);

    pthread_mutex_lock(&sch_mutex);
    enqueue(&sch->ready_queues[priority], tr);
    fprintf(stderr, "Placed thread %lu in ready q\n", ptr);
    pthread_mutex_unlock(&sch_mutex);
    fprintf(stderr, "Unlocked mutex after placed thread in ready\n");



    check_scheduler();

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