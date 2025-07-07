// pcb.h
#ifndef PCB_H
#define PCB_H

#define MEMORY_SIZE 60
#define MAX_LINE_LENGTH 100

// Enum for process states
typedef enum {
    NEW,
    READY,
    RUNNING,
    BLOCKED,
    TERMINATED
} ProcessState;

// Structure for Process Control Block
typedef struct PCB {
    int process_id;
    ProcessState state;
    int priority;
    int program_counter;
    int memory_lower_bound;
    int memory_upper_bound;
} PCB;

// External memory array
extern char memory[MEMORY_SIZE][MAX_LINE_LENGTH];

// Function declarations
PCB* create_pcb(int process_id, int priority, int memory_lower, int memory_upper);
void update_pcb_state(PCB* pcb, ProcessState new_state);
void update_pcb_priority(PCB* pcb, int new_priority);
void update_pcb_program_counter(PCB* pcb, int new_pc);
void free_pcb(PCB* pcb);
void freeProgram(int pid); // Added declaration for freeProgram
void print_pcb(PCB* pcb);
int findPCBStartIndex(int pid);

#endif