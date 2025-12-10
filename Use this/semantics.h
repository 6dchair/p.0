#ifndef SEMANTICS_H
#define SEMANTICS_H

#include <stdbool.h>
#include "ast.h"

// symbol table entry
typedef struct Symbol {
    char *name;
    int declared_line;
    bool initialized;
    struct Symbol *next;
} Symbol;

// semantic analyzer state
typedef struct Semantics {
    Symbol *symbol_table;
    int current_line;
    int error_count;
    bool in_decl_line;  // are we parsing a declaration line?
} Semantics;

// initialize semantic analyzer
void sem_init(Semantics *sem);

// set current line number
void sem_set_line(Semantics *sem, int line);

// set declaration line flag
void sem_set_decl_line(Semantics *sem, bool is_decl_line);

// check if variable is declared before use
bool sem_check_declared(Semantics *sem, const char *name);

// add a new symbol (declaration)
bool sem_add_symbol(Semantics *sem, const char *name);

// check for duplicate declaration
bool sem_is_duplicate(Semantics *sem, const char *name);

// get error count
int sem_get_error_count(Semantics *sem);

// print symbol table (for debugging)
void sem_print_symbols(Semantics *sem);

// clean up
void sem_cleanup(Semantics *sem);

// add to semantics.h
bool sem_check_division_by_zero(Node *expr_node);

#endif