#ifndef MUTEX_H
#define MUTEX_H

#include "queue.h"

#define MAX_HOLD_CYCLES 5

typedef struct {
    int mutex_id;          // Unique identifier for the mutex
    bool locked;           // True if the mutex is locked
    int ownerPID;          // PID of the process that owns the mutex
    int hold_cycles;       // Number of cycles the mutex has been held
    int preemption_flag;   // Flag to indicate if preemption occurred
    Queue blockedQueue;    // Queue of processes blocked on this mutex
} Mutex;

// External mutexes
extern Mutex mutexFile;
extern Mutex mutexInput;
extern Mutex mutexOutput;

// Function prototypes
void initMutexes();
bool semWait(Mutex* mutex, int pid, char (*memory)[60][100]);
void semSignal(Mutex* mutex);
void releaseMutexOnTermination(Mutex* mutex, int pid);
void blockProcess(int pid);
void unblockProcess(int pid);
int getProcessPriority(int pid);
void checkPreemption(Mutex* mutex);

#endif