#ifndef SO_SCHEDULER_H_
#include "so_scheduler.h"
#endif

#define MAX_SIZE 100
#define TYPES

typedef struct {
    void* elem[MAX_SIZE];
    int front;
    int rear;
    int no_elem;
} C_Queue;

typedef struct {
    unsigned int time_quantum;
    unsigned int priority;
    tid_t tid;
    so_handler* func;
    int work_done;
} thread;

typedef struct {
    unsigned int time_quantum;
    unsigned int io;
    unsigned int no_threads;
    unsigned int no_terminated_threads;
    thread* running_thread;
    thread* terminated_threads[MAX_SIZE];
    C_Queue ready_queues[SO_MAX_PRIO + 1];
    C_Queue waiting_queues[SO_MAX_NUM_EVENTS + 1];
} scheduler;