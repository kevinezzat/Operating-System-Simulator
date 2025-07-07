#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "pcb.h"
#include "instruction.h"
#include "queue.h"
#include "mutex.h"
#include "gui.h"

#define CLOCK_CYCLES_PER_INSTRUCTION 1
#define NUM_QUEUES 4
#define TIME_QUANTUM_0 1
#define TIME_QUANTUM_1 2
#define TIME_QUANTUM_2 4
#define TIME_QUANTUM_3 8
#define RR_TIME_QUANTUM 4
#define MEMORY_SIZE 60
#define MAX_LINE_LENGTH 100
#define MAX_CYCLES 100
#define DEADLOCK_THRESHOLD 5
#define MAX_DEADLOCK_ATTEMPTS 3

char memory[MEMORY_SIZE][MAX_LINE_LENGTH];
int availableMemory = MEMORY_SIZE;
int deadlock_attempts[MAX_PROCESSES] = {0};
int quantaCount;
SimulationState sim_state;
Queue unBlockedQueue;

int countInstructions(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        append_log(&sim_state, g_strdup_printf("Error opening file: %s", filename));
        return -1;
    }

    char line[MAX_LINE_LENGTH];
    int count = 0;

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) > 0) {
            count++;
        }
    }

    fclose(file);
    return count;
}

int loadProgram(const char* filename, int pid) {
    if (pid <= 0 || sim_state.numProcesses >= MAX_PROCESSES) {
        append_log(&sim_state, g_strdup_printf("Invalid PID %d or max processes reached", pid));
        return -1;
    }
    int totalInstructions = countInstructions(filename);
    if (totalInstructions <= 0) return -1;

    int totalNeeded = 6 + totalInstructions + 3;
    if (totalNeeded > availableMemory) {
        append_log(&sim_state, g_strdup_printf("Not enough memory: Needed %d, Available %d", totalNeeded, availableMemory));
        return -1;
    }

    FILE* file = fopen(filename, "r");
    if (!file) {
        append_log(&sim_state, g_strdup_printf("Error opening file: %s", filename));
        return -1;
    }

    int lowerBound = MEMORY_SIZE - availableMemory;
    snprintf(memory[lowerBound], MAX_LINE_LENGTH, "pid : %d", pid);
    snprintf(memory[lowerBound + 1], MAX_LINE_LENGTH, "state : Ready");
    snprintf(memory[lowerBound + 2], MAX_LINE_LENGTH, "priority : 1");
    snprintf(memory[lowerBound + 3], MAX_LINE_LENGTH, "pc : %d", lowerBound + 6);
    snprintf(memory[lowerBound + 4], MAX_LINE_LENGTH, "lowerBound : %d", lowerBound);

    availableMemory -= 6;

    char line[MAX_LINE_LENGTH];
    int currentIndex = lowerBound + 6;

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) == 0) continue;

        snprintf(memory[currentIndex], MAX_LINE_LENGTH, "%s", line);
        currentIndex++;
        availableMemory--;
    }

    fclose(file);

    snprintf(memory[currentIndex], MAX_LINE_LENGTH, "Empty");
    snprintf(memory[currentIndex + 1], MAX_LINE_LENGTH, "Empty");
    snprintf(memory[currentIndex + 2], MAX_LINE_LENGTH, "Empty");

    availableMemory -= 3;
    snprintf(memory[lowerBound + 5], MAX_LINE_LENGTH, "upperBound : %d", currentIndex + 2);

    // Update simulation state
    ProcessInfo *info = &sim_state.processes[sim_state.numProcesses++];
    info->pid = pid;
    strcpy(info->state, "Ready");
    info->priority = 1;
    info->lowerBound = lowerBound;
    info->upperBound = currentIndex + 2;
    info->pc = lowerBound + 6;
    info->arrivalTime = 0;
    strcpy(info->currentInstruction, "");
    info->timeInQueue = 0;

    memcpy(sim_state.memory, memory, sizeof(memory));
    update_gui(&sim_state);
    return 0;
}

void updateVariable(int pid, const char* variableName, const char* value) {
    int pcbIndex = findPCBStartIndex(pid);
    if (pcbIndex == -1) {
        append_log(&sim_state, g_strdup_printf("PID %d not found", pid));
        return;
    }

    int upperBound;
    sscanf(memory[pcbIndex + 5], "upperBound : %d", &upperBound);
    //append_log(&sim_state, g_strdup_printf("PID %d: updateVariable searching region [%d, %d] for %s", pid, lowerBound, upperBound, variableName));

    for (int i = upperBound-2; i <= upperBound; i++) {
        if (strncmp(memory[i], variableName, strlen(variableName)) == 0 && memory[i][strlen(variableName)] == ' ') {
            snprintf(memory[i], MAX_LINE_LENGTH, "%s : %s", variableName, value);
            memcpy(sim_state.memory, memory, sizeof(memory));
            update_gui(&sim_state);
            append_log(&sim_state, g_strdup_printf("PID %d: Updated variable %s = %s at slot %d", pid, variableName, value, i));
            return;
        }
    }

    // If not found, create it in the first empty slot
    for (int i = upperBound-3; i <= upperBound; i++) {
        if (strlen(memory[i]) == 0 || strcmp(memory[i], "Empty") == 0) {
            snprintf(memory[i], MAX_LINE_LENGTH, "%s : %s", variableName, value);
            memcpy(sim_state.memory, memory, sizeof(memory));
            update_gui(&sim_state);
            append_log(&sim_state, g_strdup_printf("PID %d: Created variable %s = %s at slot %d", pid, variableName, value, i));
            return;
        }
    }

    append_log(&sim_state, g_strdup_printf("PID %d: No empty variable slot for %s", pid, variableName));
}

char* getVariableValue(int pid, const char* variableName) {
    char value[50];
    char* returnValue;
    returnValue = malloc(MAX_LINE_LENGTH);
    int pcbIndex = findPCBStartIndex(pid);
    if (pcbIndex == -1) {
        append_log(&sim_state, g_strdup_printf("PID %d: Process not found for %s", pid, variableName));
        return NULL;
    }

    int upperBound;
    sscanf(memory[pcbIndex + 5], "upperBound : %d", &upperBound);
    //append_log(&sim_state, g_strdup_printf("PID %d: getVariableValue searching region [%d, %d] for %s", pid, lowerBound, upperBound, variableName));

    for (int i = upperBound - 2; i <= upperBound; i++) {
        if (strncmp(memory[i], variableName, strlen(variableName)) == 0 && memory[i][strlen(variableName)] == ' ') {
            if (sscanf(memory[i], "%*s : %s", value) == 1) {
                append_log(&sim_state, g_strdup_printf("PID %d: Found %s = '%s' at slot %d", pid, variableName, value, i));
                strcpy(returnValue, value);
                return returnValue;
            } else {
                append_log(&sim_state, g_strdup_printf("PID %d: %s empty at slot %d", pid, variableName, i));
                return "";
            }
        }
    }
    append_log(&sim_state, g_strdup_printf("PID %d: %s not found in variable slots", pid, variableName));
    return NULL;
}

void executeInstruction(int pid, const char* command, const char* arg1, const char* arg2, Queue* queues) {
    InstructionType type = getInstructionType(command);
    if (type == INVALID) {
        append_log(&sim_state, g_strdup_printf("Invalid instruction: %s", command));
        return;
    }

    if (strcmp(command, "assign") == 0 && strncmp(arg2, "readFile", 8) == 0) { 
        char filename[50];
        memset(filename, 0, sizeof(filename));
        if (sscanf(arg2, "readFile %s", filename) != 1) {
            append_log(&sim_state, g_strdup_printf("PID %d: Invalid readFile format: %s", pid, arg2));
            return;
        }
        char* file_value = getVariableValue(pid, filename);
        if (!file_value) {
            append_log(&sim_state, g_strdup_printf("PID %d: Variable %s not found", pid, filename));
            return;
        }
        FILE* file = fopen(file_value, "r");
        if (file == NULL) {
            append_log(&sim_state, g_strdup_printf("PID %d: Cannot open file '%s'", pid, file_value));
            return;
        }
        append_log(&sim_state, g_strdup_printf("PID %d: Reading file '%s'", pid, file_value));

        char line[100];
        while (fgets(line, sizeof(line), file)) {
            append_log(&sim_state, g_strdup_printf("PID %d: Read %s from %s", pid, line, file_value));
        }
        fclose(file);
        updateVariable(pid, arg1, line);
        return;
    }

    switch (type) {
        case PRINT: {
            char* value = getVariableValue(pid, arg1);
            if (value && strlen(value) > 0) {
                append_log(&sim_state, g_strdup_printf("PID %d: Print %s = %s", pid, arg1, value));
            } else if (value) {
                append_log(&sim_state, g_strdup_printf("PID %d: %s empty", pid, arg1));
            } else {
                append_log(&sim_state, g_strdup_printf("PID %d: Print literal %s", pid, arg1));
            }
            break;
        }

        case ASSIGN: {
            if (strcmp(arg2, "input") == 0) {
                append_log(&sim_state, g_strdup_printf("PID %d: Waiting for input for %s", pid, arg1));
                sim_state.waiting_for_input_pid = pid;
                strncpy(sim_state.waiting_for_input_var, arg1, sizeof(sim_state.waiting_for_input_var)-1);
                sim_state.waiting_for_input_var[sizeof(sim_state.waiting_for_input_var)-1] = '\0';
                // Do not block here; input will be handled by the GUI
                return;
            } else {
                updateVariable(pid, arg1, arg2);
                append_log(&sim_state, g_strdup_printf("PID %d: Assigned %s = %s", pid, arg1, arg2));
            }
            break;
        }

        case WRITE_FILE: {
            char* fileName = getVariableValue(pid, arg1);
            FILE* file = fopen(fileName, "w");
            if (file == NULL) {
                append_log(&sim_state, g_strdup_printf("PID %d: Cannot open file %s", pid, arg1));
                break;
            }

            char* value = getVariableValue(pid, arg2);
            if (value) {
                fprintf(file, "%s", value);
                append_log(&sim_state, g_strdup_printf("PID %d: Wrote %s = '%s' to %s", pid, arg2, value, arg1));
            } else {
                append_log(&sim_state, g_strdup_printf("PID %d: Variable %s not found", pid, arg2));
            }
            fclose(file);
            break;
        }

        case READ_FILE: {
            char* file_value = getVariableValue(pid, arg1);
            if (!file_value) {
                append_log(&sim_state, g_strdup_printf("PID %d: Variable %s not found", pid, arg1));
                break;
            }

            FILE* file = fopen(file_value, "r");
            if (file == NULL) {
                append_log(&sim_state, g_strdup_printf("PID %d: Cannot open file %s", pid, file_value));
                break;
            }

            char line[100];
            while (fgets(line, sizeof(line), file)) {
                append_log(&sim_state, g_strdup_printf("PID %d: Read %s from %s", pid, line, file_value));
            }
            fclose(file);
            break;
        }

        case PRINT_FROM_TO: {
            int start, end;
            if (sscanf(arg1, "%d", &start) != 1)
                start = atoi(getVariableValue(pid, arg1));
            if (sscanf(arg2, "%d", &end) != 1)
                end = atoi(getVariableValue(pid, arg2));
            char output[200] = "";
            for (int i = start; i <= end; i++) {
                char num[10];
                snprintf(num, sizeof(num), "%d ", i);
                strcat(output, num);
            }
            append_log(&sim_state, g_strdup_printf("PID %d: Print from %d to %d: %s", pid, start, end, output));
            break;
        }

        case SEM_WAIT: {
            Mutex* mutex = NULL;
            int mutex_id = -1;
            if (strcmp(arg1, "userInput") == 0) { mutex = &mutexInput; mutex_id = 0; }
            else if (strcmp(arg1, "userOutput") == 0) { mutex = &mutexOutput; mutex_id = 2; }
            else if (strcmp(arg1, "file") == 0) { mutex = &mutexFile; mutex_id = 1; }
            if (mutex) {
                if (semWait(mutex, pid, &memory)) {
                    append_log(&sim_state, g_strdup_printf("PID %d: Acquired %s", pid, arg1));
                    sim_state.mutexes[mutex_id].locked = mutex->locked;
                    sim_state.mutexes[mutex_id].ownerPid = mutex->ownerPID;
                } else {
                    append_log(&sim_state, g_strdup_printf("PID %d: Blocked on %s", pid, arg1));          //----------------------------------
                    sim_state.mutexes[mutex_id].locked = mutex->locked;
                    sim_state.mutexes[mutex_id].ownerPid = mutex->ownerPID;
                    enqueue(&(mutex->blockedQueue), pid);
                }
            } else {
                append_log(&sim_state, g_strdup_printf("PID %d: Invalid mutex %s", pid, arg1));
            }
            break;
        }

        case SEM_SIGNAL: {
            Mutex* mutex = NULL;
            int mutex_id = -1;
            if (strcmp(arg1, "userInput") == 0) { mutex = &mutexInput; mutex_id = 0; }
            else if (strcmp(arg1, "userOutput") == 0) { mutex = &mutexOutput; mutex_id = 2; }
            else if (strcmp(arg1, "file") == 0) { mutex = &mutexFile; mutex_id = 1; }
            if (mutex) {
                semSignal(mutex);
                append_log(&sim_state, g_strdup_printf("PID %d: Released %s", pid, arg1));            //----------------------------
                sim_state.mutexes[mutex_id].locked = mutex->locked;
                sim_state.mutexes[mutex_id].ownerPid = mutex->ownerPID;
                if (mutex->ownerPID != -1){
                    enqueue(&unBlockedQueue, mutex->ownerPID);
                }
                
            } else {
                append_log(&sim_state, g_strdup_printf("PID %d: Invalid mutex %s", pid, arg1));
            }
            break;
        }

        case INVALID:
            append_log(&sim_state, "Invalid instruction");
            break;
    }
    memcpy(sim_state.memory, memory, sizeof(memory));
    update_gui(&sim_state);
}


int isCommand(const char* str) {
    InstructionType type = getInstructionType(str);
    return type != INVALID;
}


int executeForTimeQuantum(int pid, int* pc, int lowerBound, int upperBound, int timeQuantum, Queue* queues) {
    int instructionsExecuted = 0;
    int pcbIndex = findPCBStartIndex(pid);
    if (pcbIndex == -1) {
        append_log(&sim_state, g_strdup_printf("PID %d: Program not found", pid));
        return 0;
    }

    // Execute only one instruction
    if (*pc <= upperBound - 3) {
        char instructionLine[MAX_LINE_LENGTH];
        strcpy(instructionLine, memory[*pc]);

        char command[50];
        char arg1[50] = "";
        char arg2[50] = "";
        sscanf(instructionLine, "%s %s %s", command, arg1, arg2);

        if(strcmp(arg2,"readFile") == 0){
            sscanf(instructionLine, "%s %s %[^\n]", command, arg1, arg2);
        }

        append_log(&sim_state, g_strdup_printf("PID %d: Executing : %s", pid, instructionLine));
        for (int i = 0; i < sim_state.numProcesses; i++) {
            if (sim_state.processes[i].pid == pid) {
                strcpy(sim_state.processes[i].currentInstruction, instructionLine);
                sim_state.processes[i].pc = *pc;
                break;
            }
        }

        executeInstruction(pid, command, arg1, arg2, queues);

        char stateStr[20];
        sscanf(memory[pcbIndex + 1], "state : %s", stateStr);
        for (int i = 0; i < sim_state.numProcesses; i++) {
            if (sim_state.processes[i].pid == pid) {
                strcpy(sim_state.processes[i].state, stateStr);
                break;
            }
        }

        // if (strcmp(stateStr, "Blocked") == 0) {
        //     append_log(&sim_state, g_strdup_printf("PID %d: Blocked after instruction", pid));
        //     return 0;
        // }

        (*pc)++;
        instructionsExecuted = 1;
    }
    update_gui(&sim_state);
    return instructionsExecuted;
}

void freeProgram(int pid) {
    int pcbIndex = findPCBStartIndex(pid);
    if (pcbIndex == -1) {
        append_log(&sim_state, g_strdup_printf("PID %d: Not found for freeProgram", pid));
        return;
    }

    int lowerBound, upperBound;
    sscanf(memory[pcbIndex + 4], "lowerBound : %d", &lowerBound);
    sscanf(memory[pcbIndex + 5], "upperBound : %d", &upperBound);

    for (int i = lowerBound; i <= upperBound; i++) {
        memory[i][0] = '\0';
    }

    availableMemory += (upperBound - lowerBound + 1);
    append_log(&sim_state, g_strdup_printf("Freed PID %d, available memory: %d", pid, availableMemory));

    for (int i = 0; i < sim_state.numProcesses; i++) {
        if (sim_state.processes[i].pid == pid) {
            for (int j = i; j < sim_state.numProcesses - 1; j++) {
                sim_state.processes[j] = sim_state.processes[j + 1];
            }
            sim_state.numProcesses--;
            break;
        }
    }
    memcpy(sim_state.memory, memory, sizeof(memory));
    update_gui(&sim_state);
}

void update_simulation_state(Queue *queues, int numQueues, int runningPid) {
    sim_state.runningPid = runningPid;
    initializeQueue(&sim_state.readyQueue);
    initializeQueue(&sim_state.blockedQueue);

    // Process scheduler queues
    for (int i = 0; i < numQueues; i++) {
        Queue tempQueue;
        initializeQueue(&tempQueue);
        // Copy queue to temp to process without modifying original
        int count = countQueueElements(&queues[i]);
        for (int j = 0; j < count; j++) {
            int pid = dequeue(&queues[i]);
            if (pid > 0) {
                enqueue(&tempQueue, pid);
                enqueue(&queues[i], pid); // Restore original queue
            }
        }
        // Add temp queue elements to readyQueue
        while (!isEmpty(&tempQueue)) {
            int pid = dequeue(&tempQueue);
            enqueue(&sim_state.readyQueue, pid);
            for (int j = 0; j < sim_state.numProcesses; j++) {
                if (sim_state.processes[j].pid == pid) {
                    sim_state.processes[j].timeInQueue++;
                }
            }
        }
    }

    // Process mutex blocked queues
    Mutex* allMutexes[] = {&mutexInput, &mutexFile, &mutexOutput};
    for (int i = 0; i < 3; i++) {
        Queue tempQueue;
        initializeQueue(&tempQueue);
        int count = countQueueElements(&allMutexes[i]->blockedQueue);
        for (int j = 0; j < count; j++) {
            int pid = dequeue(&allMutexes[i]->blockedQueue);
            if (pid > 0) {
                enqueue(&tempQueue, pid);
                enqueue(&allMutexes[i]->blockedQueue, pid); // Restore original queue
            }
        }
        while (!isEmpty(&tempQueue)) {
            int pid = dequeue(&tempQueue);
            enqueue(&sim_state.blockedQueue, pid);
            for (int j = 0; j < sim_state.numProcesses; j++) {
                if (sim_state.processes[j].pid == pid) {
                    sim_state.processes[j].timeInQueue++;
                }
            }
        }
    }

    update_gui(&sim_state);
}

void mlfqSchedulerCycle(Queue queues[NUM_QUEUES]) {
    int active = 0;

    Mutex* allMutexes[] = {&mutexInput, &mutexFile, &mutexOutput};
    int numMutexes = 3;

    for (int i = 0; i < numMutexes; i++) {
        sim_state.mutexes[i].locked = allMutexes[i]->locked;
        sim_state.mutexes[i].ownerPid = allMutexes[i]->ownerPID;
    }

    // Process instructions from the highest priority non-empty queue
    for (int i = 0; i < NUM_QUEUES; i++) {
        if (isEmpty(&queues[i])) continue;

        int pid = peek(&queues[i]);
        if (pid <= 0) {
            append_log(&sim_state, g_strdup_printf("Invalid PID %d, skipping", pid));
            dequeue(&queues[i]);
            continue;
        }
        int pcbIndex = findPCBStartIndex(pid);
        if (pcbIndex == -1) {
            append_log(&sim_state, g_strdup_printf("PCB not found for PID %d", pid));
            dequeue(&queues[i]);
            continue;
        }

        char stateStr[20];
        sscanf(memory[pcbIndex + 1], "state : %s", stateStr);
        if (strcmp(stateStr, "Blocked") == 0) {
            append_log(&sim_state, g_strdup_printf("PID %d: Blocked", pid));
            active = 1;
            dequeue(&queues[i]);
            enqueue(&queues[i], pid);
            continue;
        }

        if (strcmp(stateStr, "Terminated") == 0) {
            append_log(&sim_state, g_strdup_printf("PID %d: Terminated, skipping", pid));
            dequeue(&queues[i]);
            continue;
        }

        int pc, lowerBound, upperBound, priority;
        sscanf(memory[pcbIndex + 3], "pc : %d", &pc);
        sscanf(memory[pcbIndex + 4], "lowerBound : %d", &lowerBound);
        sscanf(memory[pcbIndex + 5], "upperBound : %d", &upperBound);
        sscanf(memory[pcbIndex + 2], "priority : %d", &priority);

        int timeQuantum;
        switch (i) {
            case 0: timeQuantum = TIME_QUANTUM_0; break;
            case 1: timeQuantum = TIME_QUANTUM_1; break;
            case 2: timeQuantum = TIME_QUANTUM_2; break;
            case 3: timeQuantum = TIME_QUANTUM_3; break;
            default: timeQuantum = TIME_QUANTUM_3;
        }

        // Track instructions executed for this process in this queue
        static int executed[MAX_PROCESSES] = {0};
        if (executed[pid] == 0) {
            append_log(&sim_state, g_strdup_printf("Executing PID %d from Queue %d [PC=%d, TQ=%d]", pid, i, pc, timeQuantum));
        }

        snprintf(memory[pcbIndex + 1], MAX_LINE_LENGTH, "state : Running");
        sim_state.runningPid = pid;

        // Execute one instruction
        int result = executeForTimeQuantum(pid, &pc, lowerBound, upperBound, timeQuantum, queues);
        executed[pid] += result;

        sim_state.clockCycle++;
        append_log(&sim_state, g_strdup_printf("MLFQ Cycle %d", sim_state.clockCycle));

        snprintf(memory[pcbIndex + 3], MAX_LINE_LENGTH, "pc : %d", pc);

        sscanf(memory[pcbIndex + 1], "state : %s", stateStr);
        if (strcmp(stateStr, "Blocked") == 0) {
            append_log(&sim_state, g_strdup_printf("PID %d: Blocked after instruction", pid));
            active = 1;
            dequeue(&queues[i]);
            enqueue(&queues[i], pid);
            sim_state.runningPid = 0;
            executed[pid] = 0; // Reset execution count on block
            break;
        }

        if (pc > upperBound - 3) {
            append_log(&sim_state, g_strdup_printf("PID %d: Finished after instruction", pid));
            snprintf(memory[pcbIndex + 1], MAX_LINE_LENGTH, "state : Terminated");
            releaseMutexOnTermination(&mutexFile, pid);
            releaseMutexOnTermination(&mutexInput, pid);
            releaseMutexOnTermination(&mutexOutput, pid);
            freeProgram(pid);
            dequeue(&queues[i]);
            sim_state.runningPid = 0;
            executed[pid] = 0; // Reset execution count on termination
            break;
        }

        active = 1;
        int targetQueue = i;
        if (executed[pid] >= timeQuantum && i < NUM_QUEUES - 1) {
            targetQueue = i + 1;
            append_log(&sim_state, g_strdup_printf("Demoting PID %d to Queue %d", pid, targetQueue));
            executed[pid] = 0; // Reset execution count on demotion
        }

        dequeue(&queues[i]);
        enqueue(&queues[targetQueue], pid);
        sim_state.runningPid = 0;

        // Update simulation state and GUI after each instruction
        update_simulation_state(queues, NUM_QUEUES, sim_state.runningPid);
        break; // Process only one process per cycle
    }

    if (sim_state.clockCycle >= MAX_CYCLES) {
        append_log(&sim_state, g_strdup_printf("Reached max cycles (%d). Possible deadlock", MAX_CYCLES));
        active = 0;
    }

    int totalBlocked = 0;
    for (int i = 0; i < numMutexes; i++) {
        totalBlocked += countQueueElements(&allMutexes[i]->blockedQueue);
    }
    if (!active && totalBlocked > 0 && sim_state.clockCycle > DEADLOCK_THRESHOLD) {
        append_log(&sim_state, g_strdup_printf("Deadlock: %d processes blocked", totalBlocked));
        for (int i = 0; i < numMutexes; i++) {
            Queue tempQueue;
            initializeQueue(&tempQueue);
            int count = countQueueElements(&allMutexes[i]->blockedQueue);
            for (int j = 0; j < count; j++) {
                int pid = dequeue(&allMutexes[i]->blockedQueue);
                if (pid > 0) {
                    enqueue(&tempQueue, pid);
                    enqueue(&allMutexes[i]->blockedQueue, pid); // Restore original queue
                }
            }
            while (!isEmpty(&tempQueue)) {
                int pid = dequeue(&tempQueue);
                enqueue(&sim_state.mutexes[i].blockedQueue, pid);
            }
            while (!isEmpty(&allMutexes[i]->blockedQueue)) {
                int pid = dequeueHighestPriority(&allMutexes[i]->blockedQueue);
                if (pid <= 0) continue;
                int pcbIndex = findPCBStartIndex(pid);
                if (pcbIndex == -1) continue;
                char stateStr[20];
                sscanf(memory[pcbIndex + 1], "state : %s", stateStr);
                if (strcmp(stateStr, "Terminated") == 0) continue;
                deadlock_attempts[pid]++;
                if (deadlock_attempts[pid] >= MAX_DEADLOCK_ATTEMPTS) {
                    append_log(&sim_state, g_strdup_printf("PID %d: Exceeded deadlock attempts, terminating", pid));
                    snprintf(memory[pcbIndex + 1], MAX_LINE_LENGTH, "state : Terminated");
                    releaseMutexOnTermination(&mutexFile, pid);
                    releaseMutexOnTermination(&mutexInput, pid);
                    releaseMutexOnTermination(&mutexOutput, pid);
                    freeProgram(pid);
                    for (int k = 0; k < NUM_QUEUES; k++) {
                        dequeueByPID(&queues[k], pid);
                    }
                    continue;
                }
                unblockProcess(pid);
                int priority = getProcessPriority(pid);
                int targetQueue = (priority >= 0 && priority < NUM_QUEUES) ? priority : 1;
                enqueue(&queues[targetQueue], pid);
                append_log(&sim_state, g_strdup_printf("PID %d: Unblocked to Queue %d", pid, targetQueue));
                active = 1;
            }
        }
    }

    update_simulation_state(queues, NUM_QUEUES, sim_state.runningPid);
    if (!active) {
        append_log(&sim_state, "All processes finished (MLFQ)");
    }
}

void rrSchedulerCycle(Queue *queue) {
    int active = 0;
    sim_state.clockCycle++;

    Mutex* allMutexes[] = {&mutexInput, &mutexFile, &mutexOutput};
    int numMutexes = 3;

    for (int i = 0; i < numMutexes; i++) {
        sim_state.mutexes[i].locked = allMutexes[i]->locked;
        sim_state.mutexes[i].ownerPid = allMutexes[i]->ownerPID;
    }

    for (int i = 0; i < countQueueElements(&unBlockedQueue); i++){
        enqueue(queue, dequeue(&unBlockedQueue));
    }

    append_log(&sim_state, g_strdup_printf("RR Cycle %d", sim_state.clockCycle));

    if (isEmpty(queue)) return;

    int pid = peek(queue);
    if (pid <= 0) {
        append_log(&sim_state, g_strdup_printf("Invalid PID %d, skipping", pid));
        dequeue(queue);
        rrSchedulerCycle(queue);
        return;
    }
    int pcbIndex = findPCBStartIndex(pid);
    if (pcbIndex == -1) {
        append_log(&sim_state, g_strdup_printf("PCB not found for PID %d", pid));
        dequeue(queue);
        rrSchedulerCycle(queue);
        return;
    }

    char stateStr[20];
    sscanf(memory[pcbIndex + 1], "state : %s", stateStr);
    if (strcmp(stateStr, "Blocked") == 0) {
        append_log(&sim_state, g_strdup_printf("PID %d: Blocked", pid));
        active = 1;
        dequeue(queue);
        rrSchedulerCycle(queue);
        return;
    }

    if (strcmp(stateStr, "Terminated") == 0) {
        append_log(&sim_state, g_strdup_printf("PID %d: Terminated, skipping", pid));
        dequeue(queue);
        rrSchedulerCycle(queue);
        return;
    }

    int pc, lowerBound, upperBound, priority;
    sscanf(memory[pcbIndex + 3], "pc : %d", &pc);
    sscanf(memory[pcbIndex + 4], "lowerBound : %d", &lowerBound);
    sscanf(memory[pcbIndex + 5], "upperBound : %d", &upperBound);
    sscanf(memory[pcbIndex + 2], "priority : %d", &priority);

    append_log(&sim_state, g_strdup_printf("Executing PID %d [PC=%d, TQ=%d]", pid, pc, sim_state.rrQuantum));

    sim_state.runningPid = pid;

    executeForTimeQuantum(pid, &pc, lowerBound, upperBound, sim_state.rrQuantum, queue);
    snprintf(memory[pcbIndex + 3], MAX_LINE_LENGTH, "pc : %d", pc);
    
    sscanf(memory[pcbIndex + 1], "state : %s", stateStr);
    if (strcmp(stateStr, "Blocked") == 0) {
        append_log(&sim_state, g_strdup_printf("PID %d: Blocked", pid));
        active = 1;
        dequeue(queue);
        sim_state.runningPid = 0;
        return;
    }

    if (pc > upperBound - 3) {
        append_log(&sim_state, g_strdup_printf("PID %d: Finished", pid));
        snprintf(memory[pcbIndex + 1], MAX_LINE_LENGTH, "state : Terminated");
        releaseMutexOnTermination(&mutexFile, pid);
        releaseMutexOnTermination(&mutexInput, pid);
        releaseMutexOnTermination(&mutexOutput, pid);
        freeProgram(pid);
        dequeue(queue);
        sim_state.runningPid = 0;
        return;
    }
    if (quantaCount == 1){
        active = 1;
        append_log(&sim_state, g_strdup_printf("Re-enqueuing PID %d after instruction", pid));
        dequeue(queue);
        enqueue(queue, pid);
        quantaCount = sim_state.rrQuantum;
        return;
    }
        active = 1;
        quantaCount--;
    

    sim_state.runningPid = 0;


    //             if (pid > 0) {
    //                 enqueue(&tempQueue, pid);
    //                 enqueue(&allMutexes[i]->blockedQueue, pid); // Restore original queue
    //             }
    //         }
    //         while (!isEmpty(&tempQueue)) {
    //             int pid = dequeue(&tempQueue);
    //             enqueue(&sim_state.mutexes[i].blockedQueue, pid);
    //         }
    //         while (!isEmpty(&allMutexes[i]->blockedQueue)) {
    //             int pid = dequeueHighestPriority(&allMutexes[i]->blockedQueue);
    //             if (pid <= 0) continue;
    //             int pcbIndex = findPCBStartIndex(pid);
    //             if (pcbIndex == -1) continue;
    //             char stateStr[20];
    //             sscanf(memory[pcbIndex + 1], "state : %s", stateStr);
    //             if (strcmp(stateStr, "Terminated") == 0) continue;
    //             deadlock_attempts[pid]++;
    //             if (deadlock_attempts[pid] >= MAX_DEADLOCK_ATTEMPTS) {
    //                 append_log(&sim_state, g_strdup_printf("PID %d: Exceeded deadlock attempts, terminating", pid));
    //                 snprintf(memory[pcbIndex + 1], MAX_LINE_LENGTH, "state : Terminated");
    //                 releaseMutexOnTermination(&mutexFile, pid);
    //                 releaseMutexOnTermination(&mutexInput, pid);
    //                 releaseMutexOnTermination(&mutexOutput, pid);
    //                 freeProgram(pid);
    //                 dequeue(queue);
    //                 continue;
    //             }
    //             unblockProcess(pid);
    //             enqueue(queue, pid);
    //             append_log(&sim_state, g_strdup_printf("PID %d: Unblocked to Queue", pid));
    //             active = 1;
    //         }
    //     }
    // }

    update_simulation_state(queue, 1, sim_state.runningPid);
    if (!active) {
        append_log(&sim_state, "All processes finished (Round-Robin)");
    }
}

void fcfsSchedulerCycle(Queue *queue) {
    int active = 0;
    sim_state.clockCycle++;

    Mutex* allMutexes[] = {&mutexInput, &mutexFile, &mutexOutput};
    int numMutexes = 3;

    for (int i = 0; i < numMutexes; i++) {
        sim_state.mutexes[i].locked = allMutexes[i]->locked;
        sim_state.mutexes[i].ownerPid = allMutexes[i]->ownerPID;
    }

    append_log(&sim_state, g_strdup_printf("FCFS Cycle %d", sim_state.clockCycle));

    if (isEmpty(queue)) return;

    int pid = peek(queue);
    if (pid <= 0) {
        append_log(&sim_state, g_strdup_printf("Invalid PID %d, skipping", pid));
        dequeue(queue);
        return;
    }
    int pcbIndex = findPCBStartIndex(pid);
    if (pcbIndex == -1) {
        append_log(&sim_state, g_strdup_printf("PCB not found for PID %d", pid));
        dequeue(queue);
        return;
    }

    char stateStr[20];
    sscanf(memory[pcbIndex + 1], "state : %s", stateStr);
    if (strcmp(stateStr, "Blocked") == 0) {
        append_log(&sim_state, g_strdup_printf("PID %d: Blocked", pid));
        active = 1;
        dequeue(queue);
        enqueue(queue, pid);
        return;
    }

    if (strcmp(stateStr, "Terminated") == 0) {
        append_log(&sim_state, g_strdup_printf("PID %d: Terminated, skipping", pid));
        dequeue(queue);
        return;
    }

    int pc, lowerBound, upperBound, priority;
    sscanf(memory[pcbIndex + 3], "pc : %d", &pc);
    sscanf(memory[pcbIndex + 4], "lowerBound : %d", &lowerBound);
    sscanf(memory[pcbIndex + 5], "upperBound : %d", &upperBound);
    sscanf(memory[pcbIndex + 2], "priority : %d", &priority);

    append_log(&sim_state, g_strdup_printf("Executing PID %d [PC=%d, TQ=Unlimited]", pid, pc));

    snprintf(memory[pcbIndex + 1], MAX_LINE_LENGTH, "state : Running");
    sim_state.runningPid = pid;
    int executed = executeForTimeQuantum(pid, &pc, lowerBound, upperBound, INT_MAX, queue);
    snprintf(memory[pcbIndex + 3], MAX_LINE_LENGTH, "pc : %d", pc);

    sscanf(memory[pcbIndex + 1], "state : %s", stateStr);
    if (strcmp(stateStr, "Blocked") == 0) {
        append_log(&sim_state, g_strdup_printf("PID %d: Blocked", pid));
        active = 1;
        dequeue(queue);
        enqueue(queue, pid);
        sim_state.runningPid = 0;
        return;
    }

    if (pc > upperBound - 3) {
        append_log(&sim_state, g_strdup_printf("PID %d: Finished", pid));
        snprintf(memory[pcbIndex + 1], MAX_LINE_LENGTH, "state : Terminated");
        releaseMutexOnTermination(&mutexFile, pid);
        releaseMutexOnTermination(&mutexInput, pid);
        releaseMutexOnTermination(&mutexOutput, pid);
        freeProgram(pid);
        dequeue(queue);
        sim_state.runningPid = 0;
        return;
    }

    if (executed > 0) {
        active = 1;
    }
    sim_state.runningPid = 0;



    update_simulation_state(queue, 1, sim_state.runningPid);
    if (!active) {
        append_log(&sim_state, "All processes finished (FCFS)");
    }
}

void add_process(SimulationState *state, const char *filename, int arrivalTime) {
    int pid = state->numProcesses + 1;
    if (loadProgram(filename, pid) == 0) {
        for (int i = 0; i < state->numProcesses; i++) {
            if (state->processes[i].pid == pid) {
                state->processes[i].arrivalTime = arrivalTime;
                if (arrivalTime <= state->clockCycle) {
                    enqueue(&state->readyQueue, pid);
                }
                break;
            }
        }
    }
}

void reset_simulation(SimulationState *state) {
    state->numProcesses = 0;
    state->clockCycle = 0;
    state->runningPid = 0;
    strcpy(state->schedulerType, "mlfq");
    state->rrQuantum = RR_TIME_QUANTUM;
    memset(state->processes, 0, sizeof(state->processes));
    memset(state->log, 0, sizeof(state->log));
    initializeQueue(&state->readyQueue);
    initializeQueue(&state->blockedQueue);
    for (int i = 0; i < 3; i++) {
        initializeQueue(&state->mutexes[i].blockedQueue);
        state->mutexes[i].locked = 0;
        state->mutexes[i].ownerPid = 0;
    }
    memset(memory, 0, sizeof(memory));
    availableMemory = MEMORY_SIZE;
    initMutexes();
    memcpy(state->memory, memory, sizeof(memory));
    update_gui(state);
}

void run_simulation_cycle(SimulationState *state) {
    static Queue queues[NUM_QUEUES];
    static int initialized = 0;
    if (!initialized) {
        quantaCount = sim_state.rrQuantum;
        for (int i = 0; i < NUM_QUEUES; i++) {
            initializeQueue(&queues[i]);
        }
        for (int i = 0; i < state->numProcesses; i++) {
            if (state->processes[i].arrivalTime <= state->clockCycle) {
                enqueue(&queues[0], state->processes[i].pid);
            }
        }
        initialized = 1;
    }

    // Enqueue processes that have arrived
    for (int i = 0; i < state->numProcesses; i++) {
        if (state->processes[i].arrivalTime == state->clockCycle) {
            enqueue(&queues[0], state->processes[i].pid);
            append_log(state, g_strdup_printf("PID %d: Arrived", state->processes[i].pid));
        }
    }

    if (strcmp(state->schedulerType, "mlfq") == 0) {
        mlfqSchedulerCycle(queues);
    } else if (strcmp(state->schedulerType, "rr") == 0) {
        rrSchedulerCycle(&queues[0]);
    } else if (strcmp(state->schedulerType, "fcfs") == 0) {
        fcfsSchedulerCycle(&queues[0]);
    }

    // Real-time GUI updates
    update_process_list();
    update_queue_list();
    update_overview();
}

int main(int argc, char *argv[]) {
    // Initialize simulation state
    memset(&sim_state, 0, sizeof(sim_state));
    strcpy(sim_state.schedulerType, "mlfq");
    sim_state.rrQuantum = RR_TIME_QUANTUM;
    initializeQueue(&unBlockedQueue);
    initializeQueue(&sim_state.readyQueue);
    initializeQueue(&sim_state.blockedQueue);
    for (int i = 0; i < 3; i++) {
        initializeQueue(&sim_state.mutexes[i].blockedQueue);
    }
    initMutexes();


    // Start GUI
    init_gui(argc, argv);

    return 0;
}