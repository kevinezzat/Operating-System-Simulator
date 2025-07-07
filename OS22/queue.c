#include "queue.h"
#include "pcb.h"
#include <stdio.h>

// Function to initialize the queue
void initializeQueue(Queue* q) {
    q->front = -1;
    q->rear = -1;
    q->size = 0;
    for (int i = 0; i < MAX_QUEUE_SIZE; i++) {
        q->items[i] = 0;
    }
}

// Function to check if the queue is empty
bool isEmpty(Queue* q) {
    return (q->size == 0);
}

// Function to check if the queue is full
bool isFull(Queue* q) {
    return (q->size == MAX_QUEUE_SIZE);
}

// Function to check if a PID is in the queue
bool isInQueue(Queue* q, int pid) {
    if (isEmpty(q)) return false;
    for (int i = q->front; i < q->front + q->size; i++) {
        int index = i % MAX_QUEUE_SIZE;
        if (q->items[index] == pid) {
            return true;
        }
    }
    return false;
}

// Function to add an element to the queue (Enqueue operation)
void enqueue(Queue* q, int pid) {
    if (isFull(q)) {
        printf("Queue is full, cannot enqueue PID %d\n", pid);
        return;
    }
    if (isInQueue(q, pid)) {
        printf("PID %d already in queue, skipping enqueue\n", pid);
        return;
    }
    if (isEmpty(q)) {
        q->front = 0;
    }
    q->rear = (q->rear + 1) % MAX_QUEUE_SIZE;
    q->items[q->rear] = pid;
    q->size++;
}

// Function to remove an element from the queue (Dequeue operation)
int dequeue(Queue* q) {
    if (isEmpty(q)) {
        printf("Queue is empty, cannot dequeue\n");
        return -1;
    }
    int pid = q->items[q->front];
    q->front = (q->front + 1) % MAX_QUEUE_SIZE;
    q->size--;
    if (isEmpty(q)) {
        q->front = -1;
        q->rear = -1;
    }
    return pid;
}

void enqueueBack(Queue* q, int pid){
    q->front = (q->front - 1) % MAX_QUEUE_SIZE;
    q->size++;
    q->items[q->front] = pid;
}

// Function to get the element at the front of the queue (Peek operation)
int peek(Queue* q) {
    if (isEmpty(q)) {
        printf("Queue is empty\n");
        return -1;
    }
    return q->items[q->front];
}

// Function to count elements in the queue
int countQueueElements(Queue* q) {
    return q->size;
}

// Function to print the current queue
void printQueue(Queue* q) {
    if (isEmpty(q)) {
        printf("Queue is empty\n");
        return;
    }
    printf("Current Queue: ");
    int i = q->front;
    for (int count = 0; count < q->size; count++) {
        printf("%d ", q->items[i]);
        i = (i + 1) % MAX_QUEUE_SIZE;
    }
    printf("\n");
}

// Function to remove a specific PID from the queue
bool removeFromQueue(Queue* q, int pid) {
    if (isEmpty(q)) {
        return false;
    }
    int pos = -1;
    for (int i = q->front; i < q->front + q->size; i++) {
        int index = i % MAX_QUEUE_SIZE;
        if (q->items[index] == pid) {
            pos = index;
            break;
        }
    }
    if (pos == -1) {
        return false;
    }
    for (int i = pos; i != q->rear; i = (i + 1) % MAX_QUEUE_SIZE) {
        int next = (i + 1) % MAX_QUEUE_SIZE;
        q->items[i] = q->items[next];
    }
    q->rear = (q->rear - 1 + MAX_QUEUE_SIZE) % MAX_QUEUE_SIZE;
    q->size--;
    if (isEmpty(q)) {
        q->front = -1;
        q->rear = -1;
    }
    return true;
}

// Function to enqueue with priority
void enqueuePriority(Queue* q, int pid, int priority) {
    if (isFull(q)) {
        printf("Queue is full, cannot enqueue PID %d\n", pid);
        return;
    }
    if (isInQueue(q, pid)) {
        printf("PID %d already in queue, skipping enqueuePriority\n", pid);
        return;
    }
    if (isEmpty(q)) {
        q->front = 0;
        q->rear = 0;
        q->items[0] = pid;
        q->size = 1;
        return;
    }
    int i;
    for (i = q->rear; i >= q->front; i--) {
        int currentPID = q->items[i];
        int currentPriority = getProcessPriority(currentPID);
        if (currentPriority > priority) {
            q->items[i + 1] = q->items[i];
        } else {
            break;
        }
    }
    q->items[i + 1] = pid;
    q->rear++;
    q->size++;
}

// Function to dequeue the highest-priority process
int dequeueHighestPriority(Queue* q) {
    if (isEmpty(q)) {
        printf("Queue is empty, cannot dequeue\n");
        return -1;
    }
    int highestPriority = 9999; // Assume lower number = higher priority
    int highestPriorityIndex = -1;
    int highestPriorityPID = -1;
    for (int i = q->front; i < q->front + q->size; i++) {
        int index = i % MAX_QUEUE_SIZE;
        int pid = q->items[index];
        int priority = getProcessPriority(pid);
        if (priority < highestPriority) {
            highestPriority = priority;
            highestPriorityIndex = index;
            highestPriorityPID = pid;
        }
    }
    if (highestPriorityIndex == -1) {
        return dequeue(q); // Fallback to standard dequeue
    }
    for (int i = highestPriorityIndex; i != q->rear; i = (i + 1) % MAX_QUEUE_SIZE) {
        int next = (i + 1) % MAX_QUEUE_SIZE;
        q->items[i] = q->items[next];
    }
    q->rear = (q->rear - 1 + MAX_QUEUE_SIZE) % MAX_QUEUE_SIZE;
    q->size--;
    if (isEmpty(q)) {
        q->front = -1;
        q->rear = -1;
    }
    return highestPriorityPID;
}

// Function to dequeue a specific PID
void dequeueByPID(Queue* q, int pid) {
    if (isEmpty(q)) return;
    bool found = false;
    do {
        found = false;
        int pos = -1;
        for (int i = q->front; i < q->front + q->size; i++) {
            int index = i % MAX_QUEUE_SIZE;
            if (q->items[index] == pid) {
                pos = index;
                found = true;
                break;
            }
        }
        if (pos != -1) {
            for (int i = pos; i != q->rear; i = (i + 1) % MAX_QUEUE_SIZE) {
                int next = (i + 1) % MAX_QUEUE_SIZE;
                q->items[i] = q->items[next];
            }
            q->rear = (q->rear - 1 + MAX_QUEUE_SIZE) % MAX_QUEUE_SIZE;
            q->size--;
            if (isEmpty(q)) {
                q->front = -1;
                q->rear = -1;
            }
        }
    } while (found); // Remove all instances of the PID
}

// Function to insert with priority
void insertWithPriority(Queue* q, int pid, int priority) {
    enqueuePriority(q, pid, priority);
}