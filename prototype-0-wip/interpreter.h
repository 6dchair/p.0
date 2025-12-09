#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "ast.h"
#include "output.h"
#include "error.h" // to integrate error checks and labeking

typedef struct InterpreterState InterpreterState;

//char* interpret_program(Node *program);
// update function prototype to accept ErrorState
char* interpret_program(Node *program, ErrorState *error_state);

#endif