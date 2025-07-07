#include "mutex.h"
#include "pcb.h"
#include <stdio.h>
#include <string.h>

// Initialize mutexes with explicit braces for Queue struct
Mutex mutexFile = {1, false, -1, 0, 0, {{0}, -1, -1, 0}};   // mutex_id = 1
Mutex mutexInput = {2, false, -1, 0, 0, {{0}, -1, -1, 0}};  // mutex_id = 2
Mutex mutexOutput = {3, false, -1, 0, 0, {{0}, -1, -1, 0}}; // mutex_id = 3

// External mutex array for resource ordering
Mutex* mutexes[] = {&mutexFile, &mutexInput, &mutexOutput};
int num_mutexes = 3;

// Initialize mutexes and their blocked queues
void initMutexes() {
    mutexFile.locked = false;
    mutexFile.ownerPID = -1;
    mutexFile.hold_cycles = 0;
    mutexFile.preemption_flag = 0;
    mutexInput.locked = false;
    mutexInput.ownerPID = -1;
    mutexInput.hold_cycles = 0;
    mutexInput.preemption_flag = 0;
    mutexOutput.locked = false;
    mutexOutput.ownerPID = -1;
    mutexOutput.hold_cycles = 0;
    mutexOutput.preemption_flag = 0;
    initializeQueue(&mutexFile.blockedQueue);
    initializeQueue(&mutexInput.blockedQueue);
    initializeQueue(&mutexOutput.blockedQueue);
}

// Check if a process owns a specific mutex
bool ownsMutex(int pid, int mutex_id) {
    for (int i = 0; i < num_mutexes; i++) {
        if (mutexes[i]->mutex_id == mutex_id && mutexes[i]->ownerPID == pid) {
            return true;
        }
    }
    return false;
}

// Wait on a mutex
bool semWait(Mutex* mutex, int pid, char (*memory)[60][100]) {
    if (pid <= 0) {
        printf("Invalid PID %d, ignoring semWait\n", pid);
        return false;
    }

    // Check if the process already owns the mutex
    if (mutex->ownerPID == pid && mutex->locked) {
        printf("PID %d already owns mutex (ID=%d), skipping semWait\n", pid, mutex->mutex_id);
        return true; // Allow the process to continue
    }

    if (mutex->ownerPID == -1 && !mutex->locked) {
        mutex->locked = true;
        mutex->ownerPID = pid;
        printf("PID %d acquired mutex (ID=%d)\n", pid, mutex->mutex_id);
        return true;
    } else {
        enqueue(&mutex->blockedQueue, pid);
        blockProcess(pid);
        printf("PID %d blocked, waiting for mutex (ID=%d)\n", pid, mutex->mutex_id);
        return false;
    }
}

    /*// Validate PID
    if (pid <= 0) {
        printf("Invalid PID %d, ignoring semWait\n", pid);
        return false;
    }

    // Debug: Log semWait attempt
    printf("PID %d attempting semWait on mutex (ID=%d)\n", pid, mutex->mutex_id);

    // Enforce resource ordering
    for (int i = 0; i < num_mutexes; i++) {
        if (mutexes[i]->mutex_id < mutex->mutex_id && mutexes[i]->ownerPID == pid) {
            printf("PID %d violates resource ordering, blocking\n", pid);
            blockProcess(pid);
            if (!isInQueue(&mutex->blockedQueue, pid)) {
                insertWithPriority(&mutex->blockedQueue, pid, priority);
            }
            return false;
        }
    }

    // Check if process is already blocked
    int pcbIndex = findPCBStartIndex(pid);
    if (pcbIndex != -1) {
        char stateStr[20];
        sscanf(memory[pcbIndex + 1], "state : %s", stateStr);
        if (strcmp(stateStr, "Blocked") == 0) {
            printf("PID %d is already Blocked, skipping semWait\n", pid);
            return false;
        }
    }

    // Check if PID is already in mutex's blocked queue
    if (isInQueue(&mutex->blockedQueue, pid)) {
        printf("PID %d already in mutex (ID=%d) blockedQueue, skipping\n", pid, mutex->mutex_id);
        return false;
    }

    if (!mutex->locked) {
        mutex->locked = true;
        mutex->ownerPID = pid;
        mutex->hold_cycles = 0;
        mutex->preemption_flag = 0;
        printf("PID %d acquired mutex (ID=%d)\n", pid, mutex->mutex_id);
        return true;
    } else {
        insertWithPriority(&mutex->blockedQueue, pid, priority);
        printf("PID %d blocked, waiting for mutex (ID=%d), blockedQueue: ", pid, mutex->mutex_id);
        printQueue(&mutex->blockedQueue); // Debug: Show blockedQueue
        blockProcess(pid);
        mutex->hold_cycles++; // Increment hold_cycles for waiting processes
        return false;
    }
}*/

// Signal a mutex
void semSignal(Mutex* mutex) {
    printf("semSignal called for mutex (ID=%d), ownerPID=%d\n", mutex->mutex_id, mutex->ownerPID);
    if (mutex->ownerPID != -1 && mutex->locked) {
        if (!isEmpty(&mutex->blockedQueue)) {
            int unblockedPID = dequeueHighestPriority(&mutex->blockedQueue);
            int pcbIndex = findPCBStartIndex(unblockedPID);
            if (pcbIndex == -1) {
                printf("PCB not found for unblocked PID %d, discarding\n", unblockedPID);
                return;
            }
            char stateStr[20];
            sscanf(memory[pcbIndex + 1], "state : %s", stateStr);
            if (strcmp(stateStr, "Terminated") == 0) {
                printf("PID %d already terminated, discarding\n", unblockedPID);
                return;
            }
            printf("Unblocking PID %d from mutex (ID=%d), remaining blockedQueue: ", unblockedPID, mutex->mutex_id);
            printQueue(&mutex->blockedQueue); // Debug: Show remaining blockedQueue
            mutex->ownerPID = unblockedPID;
            unblockProcess(unblockedPID);
            /*int priority = getProcessPriority(unblockedPID);
            int targetQueue = (priority >= 0 && priority < numQueues) ? priority : 1;
            enqueue(&readyQueues[targetQueue], unblockedPID);
            printf("PID %d enqueued to Queue %d\n", unblockedPID, targetQueue);*/
        } else {
            mutex->locked = false;
            mutex->ownerPID = -1;
            mutex->hold_cycles = 0;
            mutex->preemption_flag = 0;
            printf("Mutex (ID=%d) released, no processes waiting\n", mutex->mutex_id);
        }
    } else {
        printf("Mutex (ID=%d) not locked, no action taken\n", mutex->mutex_id);
    }
}

// Release mutex when a process terminates
void releaseMutexOnTermination(Mutex* mutex, int pid) {
    if (mutex->ownerPID == pid && mutex->locked) {
        printf("Releasing mutex (ID=%d) held by terminating PID %d\n", mutex->mutex_id, pid);
        semSignal(mutex);
    }
}

// Block a process by updating its state
void blockProcess(int pid) {
    int pcbIndex = findPCBStartIndex(pid);
    if (pcbIndex != -1) {
        snprintf(memory[pcbIndex + 1], MAX_LINE_LENGTH, "state : Blocked");
        printf("Process %d is now BLOCKED.\n", pid);
    }
}

// Unblock a process by updating its state
void unblockProcess(int pid) {
    int pcbIndex = findPCBStartIndex(pid);
    if (pcbIndex != -1) {
        snprintf(memory[pcbIndex + 1], MAX_LINE_LENGTH, "state : Ready");
        printf("Process %d is now UNBLOCKED.\n", pid);
    }
}

// Get the priority of a process from its PCB
int getProcessPriority(int pid) {
    int pcbIndex = findPCBStartIndex(pid);
    if (pcbIndex == -1) {
        printf("Process with PID %d not found!\n", pid);
        return 1; // Default priority
    }
    int priority;
    sscanf(memory[pcbIndex + 2], "priority : %d", &priority);
    return priority;
}

// Check for mutex preemption
/*void checkPreemption(Mutex* mutex) {
    if (mutex->locked && mutex->ownerPID != -1 && mutex->hold_cycles >= MAX_HOLD_CYCLES) {
        printf("Preempting mutex (ID=%d) held by PID %d after %d cycles\n",
               mutex->mutex_id, mutex->ownerPID, mutex->hold_cycles);
        mutex->preemption_flag = 1;
        int old_owner = mutex->ownerPID;
        mutex->locked = false;
        mutex->ownerPID = -1;
        mutex->hold_cycles = 0;
        unblockProcess(old_owner);
        int old_priority = getProcessPriority(old_owner);
        if (!isEmpty(&mutex->blockedQueue)) {
            int new_owner = dequeueHighestPriority(&mutex->blockedQueue);
            printf("Assigning mutex (ID=%d) to PID %d after preemption, remaining blockedQueue: ",
                   mutex->mutex_id, new_owner);
            printQueue(&mutex->blockedQueue); // Debug: Show blockedQueue
            mutex->locked = true;
            mutex->ownerPID = new_owner;
            mutex->hold_cycles = 0;
            unblockProcess(new_owner);
            int priority = getProcessPriority(new_owner);
            int targetQueue = (priority >= 0 && priority < NUM_QUEUES) ? priority : 1;
            enqueue(&readyQueue, new_owner);
            printf("PID %d enqueued to Queue %d\n", new_owner, targetQueue);
        }
        // Re-enqueue old owner to appropriate queue
        int targetQueue = (old_priority >= 0 && old_priority < NUM_QUEUES) ? old_priority : 1;
        enqueue(&readyQueue, old_owner);
        printf("PID %d (old owner) enqueued to Queue %d\n", old_owner, targetQueue);
    }
    if (mutex->locked && mutex->ownerPID != -1) {
        mutex->hold_cycles++;
    }
}*/