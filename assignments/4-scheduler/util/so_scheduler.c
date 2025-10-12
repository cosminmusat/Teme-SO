#include <stdlib.h>
#include <stdio.h>
#include "types.h"

static scheduler* sch = NULL;

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

    return 0;
}

DECL_PREFIX tid_t so_fork(so_handler *func, unsigned int priority) {
    // pthread_attr_t thread_attr;
    // pthread_attr_init(&thread_attr);
    // pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
    // pthread_t thread;
    // tid_t thread_id = pthread_create(&thread, NULL, func, &priority);
    // return thread_id;
    return 0;
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
    if (sch != NULL) {
        free(sch);
        sch = NULL;
    }

    return;
}