#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pcb.h"

PCB* create_pcb(int process_id, int priority, int memory_lower, int memory_upper) {
    PCB* pcb = (PCB*)malloc(sizeof(PCB));
    if (pcb == NULL) {
        printf("memory error for PCB\n");
        return NULL;
    }
    pcb->process_id = process_id;
    pcb->state = NEW;
    pcb->priority = priority;
    pcb->program_counter = 0;
    pcb->memory_lower_bound = memory_lower;
    pcb->memory_upper_bound = memory_upper;
    
    printf("Created PCB: PID=%d, Priority=%d, Memory[%d, %d]\n", 
            pcb->process_id, pcb->priority, pcb->memory_lower_bound, pcb->memory_upper_bound);
    
    return pcb;
}

void update_pcb_state(PCB* pcb, ProcessState new_state) {
    if (pcb) {
        pcb->state = new_state;
    }
}

void update_pcb_priority(PCB* pcb, int new_priority) {
    if (pcb) {
        pcb->priority = new_priority;
    }
}

void update_pcb_program_counter(PCB* pcb, int new_pc) {
    if (pcb) {
        pcb->program_counter = new_pc;
    }
}

void print_pcb(PCB* pcb) {
    if (pcb) {
        printf("PCB: ID=%d, State=%d, Priority=%d, PC=%d, Memory=[%d,%d]\n",
               pcb->process_id, pcb->state, pcb->priority, pcb->program_counter,
               pcb->memory_lower_bound, pcb->memory_upper_bound);
    }
}

void free_pcb(PCB* pcb) {
    if (pcb) {
        free(pcb);
    }
}

int findPCBStartIndex(int pid) {
    for (int i = 0; i < MEMORY_SIZE; i++) {
        if (strncmp(memory[i], "pid", 3) == 0) {
            int storedPid;
            sscanf(memory[i], "pid : %d", &storedPid);
            if (storedPid == pid) {
                return i;
            }
        }
    }
    return -1;
}