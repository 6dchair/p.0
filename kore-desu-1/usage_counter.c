#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "usage_counter.h"
#include "symbol_table.h"

// struct to store variable name and how many times it appears in thr code
typedef struct {
    char name[MAX_NAME_LEN]; // var name
    int count; // times this var is used
} VarUsage;

// static array that holds all variable usage information
// these are file-scoped (not visible to other files)
static VarUsage vars[MAX_SYMBOLS];
static int var_count = 0; // total number of unique variables tracked

// add occurrence of a variable or ncrement usage count of a var name
static void AddVariableUsage(const char *name) {
    if(!name || !*name) // ignore empty or invalid nmaes
        return;

    // ignore keywords or numbers
    if(isdigit(name[0]))
        return;

    // search if variable alr exists in the table
    for(int i = 0; i < var_count; i++) {
        if(strcmp(vars[i].name, name) == 0) {
            vars[i].count++; // found, so increment usage counter
            return;
        }
    }

    // if var is not found, add to it the list 
    if(var_count < MAX_SYMBOLS) {
        strncpy(vars[var_count].name, name, MAX_NAME_LEN-1);
        vars[var_count].name[MAX_NAME_LEN-1] = '\0';
        vars[var_count].count = 1; // first occurrence
        var_count++;
    }
}

// scan through the RHS and count var names
static void CountVarsInExpression(const char *expr) {
    char token[128];
    int i = 0;

    while(*expr) {
        if(isalnum(*expr) || *expr == '_') {
            // collect characters that form a valid var name
            token[i++] = *expr;
        } else {
            if(i > 0) {
                token[i] = '\0';
                AddVariableUsage(token); // record the var
                i = 0;
            }
        }
        expr++;
    }

    // handle last token if the string ends with a varialble
    if(i > 0) {
        token[i] = '\0';
        AddVariableUsage(token);
    }
}

// main usage analysis
// scans all parsed statements, counts var usage frequencies, sorts them from most to least used, and allocates registers based on frwquency
void AnalyzeVariableUsage(const Statement *stmts, int count) {
    var_count = 0; // reset b4 counting

    // 1) count occurrences of all vars in statements
    for(int i = 0; i < count; i++) {
        AddVariableUsage(stmts[i].lhs); // LHS vars
        CountVarsInExpression(stmts[i].rhs); // RHS vars
    }

    // 2) sort by descending frequency (bubble sort)
    for(int i = 0; i < var_count - 1; i++) {
        for(int j = i + 1; j < var_count; j++) {
            if(vars[j].count > vars[i].count) {
                VarUsage tmp = vars[i];
                vars[i] = vars[j];
                vars[j] = tmp;
            }
        }
    }

    // 3) assign registers to top N most used vars
    SymbolInit();  // reset symbol table before assignment or new register allocation
    for(int i = 0; i < var_count && i < (REG_MAX - REG_MIN + 1); i++) {
        AllocateRegisterForTheSymbol(vars[i].name);
    }

    // for debugging
    // printf("Variable frequency table:\n");
    // for(int i = 0; i < var_count; i++)
    //     printf("%s -> %d uses\n", vars[i].name, vars[i].count);
}
