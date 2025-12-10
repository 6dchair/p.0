#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "symbol_table.h"

// symbol table entry: name -> allocated register
static struct {
    char name[MAX_NAME_LEN];
    int reg;
    uint64_t offset;
} table[MAX_SYMBOLS];

// current number of symbols in the table
static int symbol_count = 0;

// next available register to allocate
static int next_reg = REG_MIN;

// next memory offset for .data variables
static uint64_t next_offset = 0x0;

// Print .data section with .space directives for all variables
void PrintDataSection(FILE *out) {
    for(int i = 0; i < symbol_count; i++) {
        fprintf(out, "%s: .space 8\n", table[i].name);
    }
}

// initialize/reset the symbol table
void SymbolInit() {
    symbol_count = 0;
    next_reg = REG_MIN;
    next_offset = 0x0;
    // clear all var names by marking them as empty strings...
    for(int i = 0; i < MAX_SYMBOLS; i++)
        table[i].name[0] = '\0';
}

// get the register number associated with a symbol
// returns -1 if symbol not found
int GetRegisterOfTheSymbol(const char *name) {
    for(int i = 0; i < symbol_count; i++) {
        if(strcmp(table[i].name, name) == 0) 
            return table[i].reg;
    }
    return -1;
}

// check if symbol exists
int SymbolExists(const char *name) {
    return GetRegisterOfTheSymbol(name) != -1;
}

// allocate a register for a new symbol
// returns the register number, or existing reg if already allocated
// returns -1 if out of table space or registers
int AllocateRegisterForTheSymbol(const char *name) {
    int r = GetRegisterOfTheSymbol(name);
    if(r != -1)
        return r; // already allocated
    
    if(symbol_count >= MAX_SYMBOLS)
        return -1; // table is full
    if(next_reg > REG_MAX)
        return -1; // out of registers
    
    // store symbol name and assigned register
    strncpy(table[symbol_count].name, name, MAX_NAME_LEN-1);
    table[symbol_count].name[MAX_NAME_LEN-1] = '\0';
    table[symbol_count].reg = next_reg;
    // assign memory offset and increment for next variable
    table[symbol_count].offset = next_offset;
    next_offset += 0x8;  // increments by 8 bytes (like eduMIPS64)
    symbol_count++;
    return next_reg++;
}

// get the memory offset associated with a symbol
// returns 0 if symbol not found
uint64_t GetOffsetOfTheSymbol(const char *name) {
    for(int i = 0; i < symbol_count; i++) {
        if (strcmp(table[i].name, name) == 0) 
            return table[i].offset;
    }
    return 0;
}

// print all symbols with registers and offsets (for debugging)
void PrintAllSymbols(FILE *out) {
    fprintf(out, "# Symbol Table\n");
    fprintf(out, "# Name\tReg\tOffset\n");
    for(int i = 0; i < symbol_count; i++) {
        fprintf(out, "# %s\tr%d\t0x%lX\n",
                table[i].name,
                table[i].reg,
                table[i].offset);
    }
    fprintf(out, "\n");
}