// instruction.h
#ifndef INSTRUCTION_H
#define INSTRUCTION_H


#define MAX_INSTRUCTION_LENGTH 100
#define MAX_PROGRAM_LINES 50

typedef enum {
    PRINT,
    ASSIGN,
    WRITE_FILE,
    READ_FILE,
    PRINT_FROM_TO,
    SEM_WAIT,
    SEM_SIGNAL,
    INVALID
} InstructionType;

typedef struct {
    InstructionType type;
    char arg1[50];
    char arg2[50];
} Instruction;

/*Instruction* create_instruction(InstructionType type, char arg1[], char arg2[]);*/
InstructionType getInstructionType(const char* command);

#endif