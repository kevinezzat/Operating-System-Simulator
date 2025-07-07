//instruction.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "instruction.h"

/*Instruction* create_instruction(InstructionType type, char arg1[], char arg2[]) {
    Instruction* instruction = (Instruction*)malloc(sizeof(Instruction));
    if (instruction == NULL) {
        printf("memory error for Instruction\n");
        return NULL;
    }
    instruction->type = type;
    strcpy(instruction->arg1, arg1);
    strcpy(instruction->arg2, arg2);

    return instruction;
}*/

InstructionType getInstructionType(const char* command) {
    if (strcmp(command, "print") == 0) return PRINT;
    if (strcmp(command, "assign") == 0) return ASSIGN;
    if (strcmp(command, "writeFile") == 0) return WRITE_FILE;
    if (strcmp(command, "readFile") == 0) return READ_FILE;
    if (strcmp(command, "printFromTo") == 0) return PRINT_FROM_TO;
    if (strcmp(command, "semWait") == 0) return SEM_WAIT;
    if (strcmp(command, "semSignal") == 0) return SEM_SIGNAL;
    return INVALID;
}