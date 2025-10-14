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
    fprintf(stderr, "Log: Entered init - thread %lx; time quantum: %u, # io: %u\n", pthread_self(), time_quantum, io);
    if (time_quantum == 0) {
        fprintf(stderr, "Err: Exited init - thread %lx! Time quantum 0\n", pthread_self());
        return -1;
    }

    if (io > SO_MAX_NUM_EVENTS) {
        fprintf(stderr, "Err: Exited init - thread %lx! Too many io's\n", pthread_self());
        return -1;
    }

    if (sch != NULL) {
        fprintf(stderr, "Err: Exited init - thread %lx! Sched struct was not cleared\n", pthread_self());
        return -1;
    }

    fprintf(stderr, "Log: Trying to allocate sched struct - thread %lx\n", pthread_self());
    sch = calloc(1, sizeof(*sch));

    if (sch == NULL) {
        fprintf(stderr, "Err: Exited init - thread %lx! Could not allocate Sched struct\n", pthread_self());
        return -1;
    }

    fprintf(stderr, "Log: Successfully allocated sched struct - thread %lx\n", pthread_self());

    fprintf(stderr, "Log: Initializing sched struct - thread %lx\n", pthread_self());

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

    fprintf(stderr,
        "Log: Initialized sched struct - thread %lx; time quantum: %u, # io: %u, # threads: %u, # terminated threads: %u, main thread tid %lx\n",
        pthread_self(), sch->time_quantum, sch->io, sch->no_threads, sch->no_terminated_threads, sch->running_thread->tid);

    fprintf(stderr, 
        main_thread_running == 1 ? "Log: Main thread running (var) - thread %lx\n" : "Err: Main thread not running (var) - thread %lx\n",
        pthread_self());

    fprintf(stderr, "Log: Initializing ready queues' elements with NULL - thread %lx\n", pthread_self());

    for (int i = 0; i <= SO_MAX_PRIO; i++) {
        init_queue(&sch->ready_queues[i]);
    }

    fprintf(stderr, "Log: Initialized ready queues' elements with NULL - thread %lx\n", pthread_self());

    fprintf(stderr, "Log: Initializing condition variables and mutexes - thread %lx!\n", pthread_self());

    pthread_mutex_init(&sch_mutex, NULL);
    pthread_cond_init(&all_threads_terminated, NULL);
    pthread_cond_init(&is_running_thread, NULL);

    fprintf(stderr, "Log: Successfully initialized condition variables and mutexes - thread %lx!\n", pthread_self());
    fprintf(stderr, "Log: Exiting init - thread %lx!\n", pthread_self());
    return 0;
}

void* thread_function(void* arg) {
    fprintf(stderr, "Log: Entered thread function wrapper - thread %lx\n", pthread_self());

    fprintf(stderr, "Log: Initializing thread struct fields - thread %lx\n", pthread_self());

    thread* tr = (thread*)arg;
    unsigned int priority = tr->priority;
    // assign tid before trying to run
    tr->tid = pthread_self();
    tr->work_done = 0;

    fprintf(stderr, "Log: Initialized thread struct fields; tid: %lx, work done: %d\n", tr->tid, tr->work_done);

    fprintf(stderr, "Log: Entering thread function - thread %lx!\n", pthread_self());
    tr->func(priority);
    fprintf(stderr, "Log: Exited thread function - thread %lx!\n", pthread_self());
    tr->work_done = 1;

    fprintf(stderr, tr->work_done == 1 ? "Log: Work done - thread %lx\n" : "Err: Work not done - thread %lx\n", pthread_self());

    fprintf(stderr, "Log: Trying to get mutex to add thread to terminated - thread %lx\n", pthread_self());
    pthread_mutex_lock(&sch_mutex);
    fprintf(stderr, "Log: Got lock to add thread to terminated - thread %lx\n", pthread_self());
    fprintf(stderr, "Log: Adding thread to terminated - thread %lx\n", pthread_self());
    sch->terminated_threads[sch->no_terminated_threads++] = tr;
    fprintf(stderr, sch->terminated_threads[sch->no_terminated_threads] == tr 
        ? "Log: Added thread to terminated - thread %lx\n" : "Err: Didn't add thread to termianted - thread %lx", pthread_self());
    fprintf(stderr, "Log: Signaling main thread to try to join - thread %lx\n", pthread_self());
    pthread_cond_signal(&all_threads_terminated);
    fprintf(stderr, "Log: Signaled main thread to try to join - thread %lx\n", pthread_self());
    fprintf(stderr, "Log: Renouncing mutex after adding thread to terminated - thread %lx\n", pthread_self());
    pthread_mutex_unlock(&sch_mutex);
    fprintf(stderr, "Log: Renounced mutex after adding thread to terminated - thread %lx\n", pthread_self());

    fprintf(stderr, "Log: Exiting thread function wrapper - thread %lx\n", pthread_self());
    return NULL;
}

void check_scheduler() {
    fprintf(stderr, "Log: Trying to get mutex in scheduler - thread %lx\n", pthread_self());
    pthread_mutex_lock(&sch_mutex);
    fprintf(stderr, "Log: Got mutex in scheduler - thread %lx\n", pthread_self());
    thread* curr = sch->running_thread;
    fprintf(stderr, "Log: Running thread is %lx\n", sch->running_thread->tid);
    if (curr->time_quantum == 0) {
        fprintf(stderr, "Log: Quantum expired for running thread %lx\n", sch->running_thread->tid);
        enqueue(&sch->ready_queues[curr->priority], curr);
        fprintf(stderr, "Log: Enqueue thread %lx in scheduler\n", sch->running_thread->tid);
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

    if (next == NULL) {
        fprintf(stderr, "Err: Failed to find thread to preempt %lx\n", sch->running_thread->tid);
        pthread_mutex_unlock(&sch_mutex);
        fprintf(stderr, "Log: Renounced mutex after scheduling\n");
        return;
    }
    else {
        fprintf(stderr, "Log: Found thread %lx to preempt thread %lx\n", next->tid, sch->running_thread->tid);
    }

    next->time_quantum = sch->time_quantum;
    sch->running_thread = next;

    pthread_mutex_unlock(&sch_mutex);
    fprintf(stderr, "Log: Renounced mutex after scheduling\n");
    return;
}

void wait_to_run() {
    fprintf(stderr, "Log: Entered wait to run function - thread %lx\n", pthread_self());
    fprintf(stderr, "Log: Trying to get mutex to check if thread can get CPU - thread %lx\n", pthread_self());
    pthread_mutex_lock(&sch_mutex);
    fprintf(stderr, "Log: Got mutex to check if thread can get CPU - c_thread: %lx vs r_thread: %lx\n", pthread_self(), sch->running_thread->tid);
    fprintf(stderr, "Log: Thread is trying to get CPU - c_thread: %lx vs r_thread: %lx\n", pthread_self(), sch->running_thread->tid);
    while (pthread_self() != sch->running_thread->tid) {
        pthread_cond_wait(&is_running_thread, &sch_mutex);
    }
    fprintf(stderr, "Log: Thread is getting CPU - c_thread: %lx vs r_thread: %lx\n", pthread_self(), sch->running_thread->tid);
    fprintf(stderr, "Log: Renouncing mutex in wait to run - thread: %lx\n", pthread_self());
    pthread_mutex_unlock(&sch_mutex);
    fprintf(stderr, "Log: Renounced mutex in wait to run - thread %lx", pthread_self());
    fprintf(stderr, "Log: Exiting wait to run function - thread %lx\n", pthread_self());
}

void decrease_quantum() {
    fprintf(stderr, "Log: Entered decrease quantum function - thread %lx\n", pthread_self());
    fprintf(stderr, "Log: Trying to get mutex to decrease quantum - thread %lx\n", pthread_self());
    pthread_mutex_lock(&sch_mutex);
    fprintf(stderr, "Log: Got mutex to decrease quantum - thread %lx\n", pthread_self());
    fprintf(stderr, "Log: Decreasing quantum - thread %lx\n", pthread_self());
    --sch->running_thread->time_quantum;
    fprintf(stderr, "Log: Decreased quantum - thread %lx\n", pthread_self());
    fprintf(stderr, "Log: Renouncing mutex in decrease quantum - thread: %lx\n", pthread_self());
    pthread_mutex_unlock(&sch_mutex);
    fprintf(stderr, "Log: Renounced mutex in decrease quantum - thread: %lx\n", pthread_self());
    fprintf(stderr, "Log: Exiting decrease quantum function - thread %lx\n", pthread_self());
}

int preempted() {
    fprintf(stderr, "Log: Entered preempted function - thread %lx\n", pthread_self());
    int ret = 0;
    fprintf(stderr, "Log: Trying to get mutex to determine if preempted - thread %lx\n", pthread_self());
    pthread_mutex_lock(&sch_mutex);
    fprintf(stderr, "Log: Got mutex to determine if preempted - thread %lx\n", pthread_self());
    if (sch->running_thread->tid != pthread_self()) {
        ret = 1;
    }
    fprintf(stderr, "Log: Renouncing mutex to determine if preempted - thread %lx\n", pthread_self());
    pthread_mutex_unlock(&sch_mutex);
    fprintf(stderr, "Log: Renounced mutex to determine if preempted - thread %lx\n", pthread_self());
    fprintf(stderr, "Log: Thread is getting preempted - c_thread: %lx vs r_thread: %lx\n", pthread_self(), sch->running_thread->tid);
    fprintf(stderr, "Log: Exiting preempted fucntion - thread %lx\n", pthread_self());
    return ret;
}

DECL_PREFIX tid_t so_fork(so_handler *func, unsigned int priority) {

    fprintf(stderr, "Log: Entered fork; func addr: %x, priority: %u - thread %lx\n", func, priority, pthread_self());

    fprintf(stderr, "Log: Entering wait to run function - thread %lx\n", pthread_self());
    wait_to_run();
    fprintf(stderr, "Log: Exited wait to run function - thread %lx\n", pthread_self());

    // do work
    if (func == 0) {

        fprintf(stderr, "Err: Exiting fork! Function address bad - thread %lx\n", pthread_self());
        return INVALID_TID;
    }

    if (priority > SO_MAX_PRIO) {
        fprintf(stderr, "Err: Exiting fork! Priority too high - thread %lx\n", pthread_self());
        return INVALID_TID;
    }

    fprintf(stderr, "Log: Allocating thread struct and initializing it - thread %lx\n", pthread_self());

    thread* tr = malloc(sizeof(thread));
    tr->time_quantum = sch->time_quantum;
    tr->priority = priority;
    tr->func = func;

    fprintf(stderr, "Log: Initialized struct; time quantum: %u, priority %u, func addr: %x - thread %lx\n",
        tr->time_quantum, tr->priority, tr->func, pthread_self());

    pthread_t ptr;
    fprintf(stderr, "Log: Trying to get lock to increase # created threads - thread %lx\n", pthread_self());
    pthread_mutex_lock(&sch_mutex);
    fprintf(stderr, "Log: Got lock to increase # created threads - thread %lx\n", pthread_self());
    fprintf(stderr, "Log: Increasing # threads from %u - thread %lx\n", sch->no_threads, pthread_self());
    sch->no_threads++;
    fprintf(stderr, "Log: Increased # threads to %u - thread %lx\n", sch->no_threads, pthread_self());
    fprintf(stderr, "Log: Renouncing lock to increase # threads - thread %lx\n", pthread_self());
    pthread_mutex_unlock(&sch_mutex);
    fprintf(stderr, "Log: Renounced lock to increase # threads - thread %lx\n", pthread_self());

    fprintf(stderr, "Log: Creating new thread - thread %lx\n", pthread_self());
    pthread_create(&ptr, NULL, thread_function, tr);
    fprintf(stderr, "Log: Created new thread with tid %lx - thread %lx\n", ptr, pthread_self());
    // end do work

    // placing thread in ready after creation
    fprintf(stderr, "Log: Trying to get lock to enqueue newly created thread - thread %lx\n", pthread_self());
    pthread_mutex_lock(&sch_mutex);
    fprintf(stderr, "Log: Got lock to enqueue newly created thread - thread %lx\n", pthread_self());
    fprintf(stderr, "Log: Enqueuing newly created thread - thread %lx\n", pthread_self());
    enqueue(&sch->ready_queues[priority], tr);
    fprintf(stderr, rear(&sch->ready_queues[priority]) == tr ? "Log: Enqueued thread - thread %lx\n" : "Err: Failed to enqueue thread", pthread_self());
    fprintf(stderr, "Log: Renouncing lock to enqueue newly created thread - thread %lx\n", pthread_self());
    pthread_mutex_unlock(&sch_mutex);
    fprintf(stderr, "Log: Renouncing lock to enqueue newly created thread - thread %lx\n", pthread_self());


    fprintf(stderr, "Log:Entering decrease quantum function - thread %lx\n", pthread_self());
    decrease_quantum();
    fprintf(stderr, "Log: Exited decrease quantum function - thread %lx\n", pthread_self());

    fprintf(stderr, "Log: Entering check scheduler function - thread %lx\n", pthread_self());
    check_scheduler();
    fprintf(stderr, "Log: Exited check scheduler function - thread %lx\n", pthread_self());

    fprintf(stderr, "Log: Entering preempted function - thread %lx\n", pthread_self());
    if (preempted()) {
        fprintf(stderr, "Log: Exiting preempted function; status preempted - thread %lx\n", pthread_self());
        // signal so new running thread can run
        // if not preempted, no reason to signal since this is running thread

        fprintf(stderr, "Log: Trying to get lock to broadcast all waiting threads - thread %lx\n", pthread_self());
        pthread_mutex_lock(&sch_mutex);
        fprintf(stderr, "Log: Got lock to broadcast all waiting threads - thread %lx\n", pthread_self());
        pthread_cond_broadcast(&is_running_thread);
        fprintf(stderr, "Log: Broadcasted all waiting threads - thread %lx\n", pthread_self());
        fprintf(stderr, "Log: Renouncing lock to broadcast waiting threads - thread %lx\n", pthread_self());
        pthread_mutex_unlock(&sch_mutex);
        fprintf(stderr, "Log: Renounced lock to broadcast waiting threads - thread %lx\n", pthread_self());
    }
    fprintf(stderr, "Log: Exiting preempted function; status NOT preempted - thread %lx\n", pthread_self());

    fprintf(stderr, "Log: Exiting fork function - thread %lx\n", pthread_self());
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
    fprintf(stderr, "Log: Entered end - thread %lx\n", pthread_self());
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