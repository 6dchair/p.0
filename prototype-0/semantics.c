#include "semantics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void sem_init(Semantics *sem) {
    sem->symbol_table = NULL;
    sem->current_line = 0;
    sem->error_count = 0;
    sem->in_decl_line = false;
}

void sem_set_line(Semantics *sem, int line) {
    sem->current_line = line;
}

void sem_set_decl_line(Semantics *sem, bool is_decl_line) {
    sem->in_decl_line = is_decl_line;
}

// in sem_check_declared and sem_add_symbol functions
bool sem_check_declared(Semantics *sem, const char *name) {
    if (sem->error_count > 0) return false; // alr has error, stop checking
    
    Symbol *s = sem->symbol_table;
    while(s) {
        if(strcmp(s->name, name) == 0) {
            return true;
        }
        s = s->next;
    }
    
    fprintf(stderr, "Semantic error at line %d: Variable '%s' used before declaration\n", 
            sem->current_line, name);
    sem->error_count = 1; // set to 1 intead of incrementing
    return false;
}

// updated to stop counting all undeclared variable errors
// & stop at the first encounetr of such erorr
bool sem_add_symbol(Semantics *sem, const char *name) {
    // check for duplicate declaration
    Symbol *s = sem->symbol_table;
    while(s) {
        if(strcmp(s->name, name) == 0) {
            if(s->is_error) {
                s->is_error = false;
                s->declared_line = sem->current_line;
                return true;
            }
            if(sem->in_decl_line) {
                // in declaration line - this is an error ( bc we can't redeclare)
                fprintf(stderr, "Semantic error at line %d: Variable '%s' already declared\n", 
                        sem->current_line, name);
                sem->error_count++;
                return false;
            }
            // not in declaration line - this might be assignment to existing var
            return true;
        }
        s = s->next;
    }
    
    // add new symbol
    Symbol *new_sym = malloc(sizeof(Symbol));
    if(!new_sym) {
        fprintf(stderr, "Memory allocation error\n");
        return false;
    }
    
    new_sym->name = strdup(name);
    new_sym->declared_line = sem->current_line;
    new_sym->initialized = false;
    new_sym->is_error = false;  // normal symbol (not error)
    new_sym->next = sem->symbol_table;
    sem->symbol_table = new_sym;
    
    return true;
}

bool sem_is_duplicate(Semantics *sem, const char *name) {
    Symbol *s = sem->symbol_table;
    while(s) {
        if(strcmp(s->name, name) == 0)
            return true;
        s = s->next;
    }
    return false;
}

int sem_get_error_count(Semantics *sem) {
    return sem->error_count;
}


void sem_print_symbols(Semantics *sem) {
    printf("\nSymbol Table\n");
    Symbol *s = sem->symbol_table;
    while(s) {
        printf("  %s (declared at line %d, initialized: %s, is_error: %s)\n",
               s->name, s->declared_line, 
               s->initialized ? "yes" : "no",
               s->is_error ? "yes" : "no"); // errro display
        s = s->next;
    }
    printf("\n");
}

void sem_cleanup(Semantics *sem) {
    Symbol *current = sem->symbol_table;
    while(current) {
        Symbol *next = current->next;
        free(current->name);
        free(current);
        current = next;
    }
    sem->symbol_table = NULL;
}


// ADDED TO ONLY ACCEPT "int" & reflect changes in parser.y
bool sem_check_type(Semantics *sem, const char *type_name) {
    if(strcmp(type_name, "int") != 0) {
        fprintf(stderr, "Semantic error at line %d: Type '%s' is not supported. Only 'int' is allowed.\n", 
                sem->current_line, type_name);
        sem->error_count++;
        return false;
    }
    return true;
}
