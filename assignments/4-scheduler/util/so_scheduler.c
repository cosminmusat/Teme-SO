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
static pthread_cond_t is_running_thread;
static int main_thread_running = 0;
static thread* main_thread;

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
    // no of all created threads
    sch->no_threads = 0;
    sch->no_terminated_threads = 0;
    // main thread
    main_thread = calloc(1, sizeof(thread));
    sch->running_thread = main_thread;
    fprintf(stderr, "self: %lu", pthread_self());
    sch->running_thread->tid = pthread_self();
    main_thread_running = 1;

    for (int i = 0; i <= SO_MAX_PRIO; i++) {
        init_queue(&sch->ready_queues[i]);
    }

    pthread_mutex_init(&sch_mutex, NULL);
    pthread_cond_init(&all_threads_terminated, NULL);
    pthread_cond_init(&is_running_thread, NULL);

    return 0;
}

void* thread_function(void* arg) {
    thread* tr = (thread*)arg;
    unsigned int priority = tr->priority;
    // assign tid before trying to run
    tr->tid = pthread_self();
    tr->work_done = 0;
    tr->func(priority);
    tr->work_done = 1;

    pthread_mutex_lock(&sch_mutex);
    sch->terminated_threads[sch->no_terminated_threads++] = tr;
    pthread_cond_signal(&all_threads_terminated);
    pthread_mutex_unlock(&sch_mutex);

    return NULL;
}

void check_scheduler() {
    pthread_mutex_lock(&sch_mutex);
    thread* curr = sch->running_thread;

    if (curr->time_quantum == 0) {
        enqueue(&sch->ready_queues[curr->priority], curr);
        curr = NULL;
    }

    thread* next = NULL;
    for (int i = SO_MAX_PRIO; i >= 0; --i) {
        thread* tr = dequeue(&sch->ready_queues[i]);
        if (tr != NULL) {
            if (curr == NULL || main_thread_running == 1) {
                main_thread_running = 0;
                next = tr;
                break;
            }
            else {
                if (tr->priority > curr->priority) {
                    enqueue(&sch->ready_queues[curr->priority], curr);
                    next = tr;
                    break;
                }
            }
        }
    }

    next->time_quantum = sch->time_quantum;
    sch->running_thread = next;

    pthread_mutex_unlock(&sch_mutex);
    return;
}

void wait_to_run() {
    pthread_mutex_lock(&sch_mutex);
    fprintf(stderr, "self: %lu, running: %lu", pthread_self(), sch->running_thread->tid);
    while (pthread_self() != sch->running_thread->tid) {
        pthread_cond_wait(&is_running_thread, &sch_mutex);
    }
    pthread_mutex_unlock(&sch_mutex);
}

void decrease_quantum() {
    pthread_mutex_lock(&sch_mutex);
    --sch->running_thread->time_quantum;
    pthread_mutex_unlock(&sch_mutex);
}

int preempted() {
    int ret = 0;
    pthread_mutex_lock(&sch_mutex);
    if (sch->running_thread->tid != pthread_self()) {
        ret = 1;
    }
    pthread_mutex_unlock(&sch_mutex);
    return ret;
}

DECL_PREFIX tid_t so_fork(so_handler *func, unsigned int priority) {

    wait_to_run();

    // do work
    if (func == 0) {
        return INVALID_TID;
    }

    if (priority > SO_MAX_PRIO) {
        return INVALID_TID;
    }

    thread* tr = malloc(sizeof(thread));
    tr->time_quantum = sch->time_quantum;
    tr->priority = priority;
    tr->func = func;

    pthread_t ptr;
    pthread_mutex_lock(&sch_mutex);
    sch->no_threads++;
    pthread_mutex_unlock(&sch_mutex);

    pthread_create(&ptr, NULL, thread_function, tr);
    // end do work

    // placing thread in ready after creation
    pthread_mutex_lock(&sch_mutex);
    enqueue(&sch->ready_queues[priority], tr);
    pthread_mutex_unlock(&sch_mutex);

    decrease_quantum();
    check_scheduler();
    if (preempted()) {
        // signal so new running thread can run
        // if not preempted, no reason to signal since this is running thread
        pthread_mutex_lock(&sch_mutex);
        pthread_cond_broadcast(&is_running_thread);
        pthread_mutex_unlock(&sch_mutex);
    }

    return ptr;
}

DECL_PREFIX int so_wait(unsigned int io) {
    wait_to_run();

    // do work

    // end do work

    check_scheduler();
    if (preempted()) {
        // signal so new running thread can run
        // if not preempted, no reason to signal since this is running thread
        pthread_mutex_lock(&sch_mutex);
        pthread_cond_broadcast(&is_running_thread);
        pthread_mutex_unlock(&sch_mutex);
    }
    return 0;
}

DECL_PREFIX int so_signal(unsigned int io) {
    wait_to_run();

    // do work

    // end do work

    check_scheduler();
    if (preempted()) {
        // signal so new running thread can run
        // if not preempted, no reason to signal since this is running thread
        pthread_mutex_lock(&sch_mutex);
        pthread_cond_broadcast(&is_running_thread);
        pthread_mutex_unlock(&sch_mutex);
    }
    return 0;
}

DECL_PREFIX void so_exec(void) {
    wait_to_run();

    // do work

    // end do work

    decrease_quantum();
    check_scheduler();
    if (preempted()) {
        // signal so new running thread can run
        // if not preempted, no reason to signal since this is running thread
        pthread_mutex_lock(&sch_mutex);
        pthread_cond_broadcast(&is_running_thread);
        pthread_mutex_unlock(&sch_mutex);
    }
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

    free(main_thread);
    free(sch);
    sch = NULL;

    pthread_mutex_destroy(&sch_mutex);
    pthread_cond_destroy(&all_threads_terminated);
    pthread_cond_destroy(&is_running_thread);

    return;
}