#ifndef GUI_H
#define GUI_H

#include <gtk/gtk.h>
#include "pcb.h"
#include "queue.h"
#include "mutex.h"

#define MAX_PROCESSES 10
#define MAX_LOG_LENGTH 10000

typedef struct {
    int pid;
    char state[20]; // Ready, Running, Blocked, Terminated
    int priority;
    int lowerBound;
    int upperBound;
    int pc;
    int arrivalTime; // New field for user-defined arrival
    char currentInstruction[MAX_LINE_LENGTH];
    int timeInQueue; // Time spent in queue
} ProcessInfo;

typedef struct {
    ProcessInfo processes[MAX_PROCESSES];
    int numProcesses;
    Queue readyQueue;
    Queue blockedQueue; // Aggregate of mutex blocked queues
    int runningPid; // PID of currently running process
    char memory[MEMORY_SIZE][MAX_LINE_LENGTH];
    int clockCycle;
    char schedulerType[10]; // mlfq, rr, fcfs
    int rrQuantum;
    struct {
        int locked;
        int ownerPid;
        Queue blockedQueue;
    } mutexes[3]; // userInput, userOutput, file
    char log[MAX_LOG_LENGTH];
    int waiting_for_input_pid;
    char waiting_for_input_var[50];
} SimulationState;

void init_gui(int argc, char *argv[]);
void update_gui(SimulationState *state);
void append_log(SimulationState *state, const char *message);
void run_simulation_cycle(SimulationState *state);
void update_process_list(void);
void update_queue_list(void);
void update_overview(void);

#endif