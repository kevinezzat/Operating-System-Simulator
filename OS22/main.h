#ifndef MAIN_H
#define MAIN_H

#include "gui.h"
#include "queue.h"
#include "mutex.h"
#include "pcb.h"

extern SimulationState sim_state;

void add_process(SimulationState *state, const char *filename, int arrivalTime);
void reset_simulation(SimulationState *state);
void run_simulation_cycle(SimulationState *state);
int loadProgram(const char *filename, int pid);
void update_simulation_state(Queue *queues, int numQueues, int runningPid);
void mlfqSchedulerCycle(Queue queues[]);
void rrSchedulerCycle(Queue *queue);
void fcfsSchedulerCycle(Queue *queue);
void updateVariable(int pid, const char* variableName, const char* value);

#endif