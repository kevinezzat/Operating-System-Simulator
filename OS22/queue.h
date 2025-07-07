#ifndef QUEUE_H
#define QUEUE_H

#include <stdbool.h>

#define MAX_QUEUE_SIZE 100  // Or any value you use throughout your code

typedef struct {
    int items[MAX_QUEUE_SIZE];
    int front;
    int rear;
    int size;  // Add size field to track number of elements
} Queue;

// Queue function declarations
void initializeQueue(Queue* q);
bool isEmpty(Queue* q);
bool isFull(Queue* q);
void enqueue(Queue* q, int pid);
int dequeue(Queue* q);
void enqueueBack(Queue* q, int pid);
int peek(Queue* q);
int countQueueElements(Queue* q);
void printQueue(Queue* q);
bool removeFromQueue(Queue* q, int pid);  // Add function to remove specific PID
void enqueuePriority(Queue* q, int pid, int priority);  // Insert sorted
int dequeueHighestPriority(Queue* q);                   // Remove highest
void dequeueByPID(Queue* q, int pid); 
void insertWithPriority(Queue* q, int pid, int priority);

#endif // QUEUE_H
