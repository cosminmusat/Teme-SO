#ifndef TYPES
#include "types.h"
#endif

#define CQUEUE

void init_queue(C_Queue* q);

int is_full(C_Queue* q);

int is_empty(C_Queue* q);

void enqueue(C_Queue* q, void* data);

void* dequeue(C_Queue* q);

void* front(C_Queue* q);