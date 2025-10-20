#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include "types.h"
#include "circular_queue.h"

static scheduler* sch = NULL;
static pthread_mutex_t sch_mutex;
static pthread_cond_t is_running_thread;
static sem_t not_terminated_threads;
static sem_t all_threads_terminated;
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
    sch->running_thread->tid = pthread_self();
    main_thread_running = 1;

    for (int i = 0; i <= SO_MAX_PRIO; i++) {
        init_queue(&sch->ready_queues[i]);
    }

    for (int i = 0; i < SO_MAX_NUM_EVENTS; i++) {
        init_queue(&sch->waiting_queues[i]);
    }

    pthread_mutex_init(&sch_mutex, NULL);
    pthread_cond_init(&is_running_thread, NULL);
    sem_init(&not_terminated_threads, 0, 0);
    sem_init(&all_threads_terminated, 0, 0);

    return 0;
}

void* thread_function(void* arg) {

    thread* tr = (thread*)arg;
    unsigned int priority = tr->priority;
    // assign tid before waiting to run
    tr->tid = pthread_self();

    wait_to_run();
    tr->work_done = 0;
    tr->func(priority);
    tr->work_done = 1;

    check_scheduler();

    if (preempted()) {
        // signal so new running thread can run
        // if not preempted, no reason to signal since this is running thread
        pthread_mutex_lock(&sch_mutex);
        pthread_cond_broadcast(&is_running_thread);
        pthread_mutex_unlock(&sch_mutex);
    }

    pthread_mutex_lock(&sch_mutex);
    sch->terminated_threads[sch->no_terminated_threads++] = tr;
    pthread_mutex_unlock(&sch_mutex);

    sem_wait(&not_terminated_threads);
    int no_not_terminated;
    sem_getvalue(&not_terminated_threads, &no_not_terminated);
    if (no_not_terminated == 0) {
        sem_post(&all_threads_terminated);
    }

    return NULL;
}

void check_scheduler() {
    pthread_mutex_lock(&sch_mutex);
    thread* curr = sch->running_thread;
    if (curr->time_quantum == 0 || curr->work_done == 1) {
        if (main_thread->tid != curr->tid && curr->work_done == 0 && curr->waiting == 0) {
            enqueue(&sch->ready_queues[curr->priority], curr);
        }
        curr = NULL;
    }

    thread* next = NULL;
    for (int i = SO_MAX_PRIO; i >= 0; --i) {
        thread* tr = front(&sch->ready_queues[i]);

        if (tr == NULL) {
            continue;
        }

        if (curr == NULL || main_thread_running == 1 || curr->waiting == 1) {
            dequeue(&sch->ready_queues[i]);
            main_thread_running = 0;
            next = tr;
            break;
        }
        else {
            if (tr->priority > curr->priority) {
                dequeue(&sch->ready_queues[i]);
                if (curr->work_done == 0 && curr->waiting == 0) {
                    enqueue(&sch->ready_queues[curr->priority], curr);
                }
                next = tr;
                break;
            }
        }
    }

    if (next == NULL) {
        pthread_mutex_unlock(&sch_mutex);
        return;
    }
    else {
    }

    next->time_quantum = sch->time_quantum;
    sch->running_thread = next;

    pthread_mutex_unlock(&sch_mutex);
    return;
}

void wait_to_run() {
    pthread_mutex_lock(&sch_mutex);
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
    tr->work_done = 0;
    tr->tid = 0;
    tr->waiting = 0;


    pthread_t ptr;
    pthread_mutex_lock(&sch_mutex);
    sch->no_threads++;
    pthread_mutex_unlock(&sch_mutex);
    sem_post(&not_terminated_threads);

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

        if (main_thread->tid != pthread_self()) {
            wait_to_run();
        }
    }

    return ptr;
}

DECL_PREFIX int so_wait(unsigned int io) {
    // do work
    pthread_mutex_lock(&sch_mutex);
    if (io >= sch->io) {
        pthread_mutex_unlock(&sch_mutex);
        return -1;
    }

    sch->running_thread->waiting = 1;
    enqueue(&sch->waiting_queues[io], sch->running_thread);
    pthread_mutex_unlock(&sch_mutex);
    // end do work

    decrease_quantum();
    check_scheduler();

    // signal so new running thread can run
    // since this thread cannot run until signaled
    pthread_mutex_lock(&sch_mutex);
    pthread_cond_broadcast(&is_running_thread);
    pthread_mutex_unlock(&sch_mutex);

    wait_to_run();
    return 0;
}

DECL_PREFIX int so_signal(unsigned int io) {

    // do work
    pthread_mutex_lock(&sch_mutex);
    if (io >= sch->io) {
        pthread_mutex_unlock(&sch_mutex);
        return -1;
    }

    thread* tr = NULL;
    int threads_signaled = 0;
    do {
        tr = dequeue(&sch->waiting_queues[io]);
        if (tr == NULL) {
            break;
        }
        tr->waiting = 0;
        threads_signaled++;
        enqueue(&sch->ready_queues[tr->priority], tr);
    } while(1);
    init_queue(&sch->waiting_queues[io]);
    pthread_mutex_unlock(&sch_mutex);
    // end do work

    decrease_quantum();
    check_scheduler();
    if (preempted()) {
        // signal so new running thread can run
        // if not preempted, no reason to signal since this is running thread
        pthread_mutex_lock(&sch_mutex);
        pthread_cond_broadcast(&is_running_thread);
        pthread_mutex_unlock(&sch_mutex);

        wait_to_run();
    }
    return threads_signaled;
}

DECL_PREFIX void so_exec(void) {
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

        wait_to_run();
    }
    return;
}

DECL_PREFIX void so_end(void) {
    if (sch == NULL) {
        return;
    }
    
    int no_not_terminated;
    sem_getvalue(&not_terminated_threads, &no_not_terminated);
    if (no_not_terminated != 0) {
        sem_wait(&all_threads_terminated);
    }

    for (unsigned int i = 0; i < sch->no_threads; i++) {
        pthread_join(sch->terminated_threads[i]->tid, NULL);
        free(sch->terminated_threads[i]);
    }

    free(main_thread);
    free(sch);
    sch = NULL;

    pthread_mutex_destroy(&sch_mutex);
    pthread_cond_destroy(&is_running_thread);
    sem_destroy(&not_terminated_threads);
    sem_destroy(&all_threads_terminated);

    return;
}