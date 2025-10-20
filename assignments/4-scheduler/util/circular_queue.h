#ifndef CIRCULAR_QUEUE_H
#define CIRCULAR_QUEUE_H
#include "types.h"

void init_queue(C_Queue* q);

int is_full(C_Queue* q);

int is_empty(C_Queue* q);

void enqueue(C_Queue* q, void* data);

void* dequeue(C_Queue* q);

void* front(C_Queue* q);

void* rear(C_Queue* q);

#endif