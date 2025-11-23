#ifndef LINE_VALIDATOR_H
#define LINE_VALIDATOR_H

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "error.h"

#define MAX_VARS 1024
#define MAX_VAR_LENGTH 100
#define BUFFER 256


// struct for variable-register pairs (reserved for future use)
typedef struct {    
    int reg;
    char var[MAX_VAR_LENGTH];
} Pair;


// global variables (shared across translation units)
extern char vars[MAX_VARS][MAX_VAR_LENGTH];
extern int valid_buffer_counter;
extern char used_vars[MAX_VARS];
extern int registers[30];
extern int used_registers[30];

// function prototypes
int IsVariableDeclared(char *variableName);
int AfterEqualsCheck(char *buffer, int *startCounter, int allowClosing);
ErrorType ParseVariableAssignment(char *buffer, int *startIndex, char *errinfo);
ErrorType StartsWithInt(char *buffer, char *errinfo);
ErrorType StartsWithVariableName(char *buffer, char *errinfo);
void RemoveLeadingAndTrailingSpaces(char *buffer);
char* RemoveAllSpaces(char *buffer, char *spacelessBuffer);

#endif

