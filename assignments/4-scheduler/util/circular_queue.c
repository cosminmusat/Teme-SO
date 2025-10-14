// C Program to implement the circular queue in c using arrays
#include <stdio.h>

#ifndef TYPES
#include "types.h"
#endif

#ifndef CQUEUE
#include "circular_queue.h"
#endif

void init_queue(C_Queue* q)
{
    q->front = -1;
    q->rear = -1;
    q->no_elem = 0;
    for (int i = 0; i < MAX_SIZE; ++i) {
        q->elem[i] = NULL;
    }
}

int is_full(C_Queue* q)
{
    return q->no_elem == MAX_SIZE;
}

int is_empty(C_Queue* q)
{
    return q->no_elem == 0;
}

void enqueue(C_Queue* q, void* data)
{
   
    if (is_full(q)) {
        return;
    }

    // If the queue is empty, set the front to the first
    // position
    if (is_empty(q)) {
        q->front = q->rear = 0;
        q->elem[q->rear] = data;
        q->no_elem = 1;
        return;
    }
    // Add the data to the queue and move the rear pointer
    q->rear = (q->rear + 1) % MAX_SIZE;
    q->elem[q->rear] = data;
    ++q->no_elem;
}

// Function to dequeue (remove) an element
void* dequeue(C_Queue* q)
{
    if (is_empty(q)) {
        return NULL;
    }
    // Get the data from the front of the queue
    void* data = q->elem[q->front];

    if (q->no_elem == 1) {
        init_queue(q);
        return data;
    }
        
    q->front = (q->front + 1) % MAX_SIZE;
    --q->no_elem;
    // Return the dequeued data
    return data;
}

void* front(C_Queue* q)
{
    if (is_empty(q)) {
        return NULL;
    }

    return q->elem[q->front];
}

void* rear(C_Queue* q)
{
    if (is_empty(q)) {
        return NULL;
    }
    return q->elem[q->rear];
}