#include <stdio.h>         
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "symbol_table.h"

// symbol table entry
typedef struct {
    char name[MAX_NAME_LEN];
    int reg; // reg assigned (-1 for labels like str0, str1 that have no register)
    uint64_t offset; // memory offset
    bool is_string; // FIX 24
    char *string_value; // FIX 24
    size_t string_len; // FIX 24
} SymbolEntry;

static SymbolEntry table[MAX_SYMBOLS];
static int symbol_count = 0;
static int next_reg = REG_MIN;
static uint64_t next_offset = 0x0;

// print .data section with .space directives
// only prits variables (those with reg != -1), str lables are habdled separately in assembly generator
// void PrintDataSection(FILE *out) {
//     for(int i = 0; i < symbol_count; i++) {
//         if(table[i].reg != -1) {
//             fprintf(out, "%s: .space 8\n", table[i].name);
//         }
//     }
// }
// FIX 24
void PrintDataSection(FILE *out) {
    for(int i = 0; i < symbol_count; i++) {
        if(table[i].reg != -1) {
            if(table[i].is_string && table[i].string_value) {
                // ch or string var: use .asciiz
                fprintf(out, "%s: .asciiz \"", table[i].name);
                // escape special characters
                for(const char *p = table[i].string_value; *p; p++) {
                    if(*p == '\n') fprintf(out, "\\n");
                    else if(*p == '\t') fprintf(out, "\\t");
                    else if(*p == '"') fprintf(out, "\\\"");
                    else if(*p == '\\') fprintf(out, "\\\\");
                    else fputc(*p, out);
                }
                fprintf(out, "\"\n");
            } else {
                // int var: use .space 8
                fprintf(out, "%s: .space 8\n", table[i].name);
            }
        }
    }
}

// initialize/reset symbol table
void SymbolInit() {
    symbol_count = 0;
    next_reg = REG_MIN;
    next_offset = 0x0;
    for(int i = 0; i < MAX_SYMBOLS; i++) {
        table[i].name[0] = '\0';
    }
}

// get register assigned to symbol
// returns -1 if symbol is a label (like str0) or not found
int GetRegisterOfTheSymbol(const char *name) {
    for(int i = 0; i < symbol_count; i++) {
        if(strcmp(table[i].name, name) == 0) {
            return table[i].reg;
        }
    }
    return -1;
}

// check if symbol exists (variable or label)
int SymbolExists(const char *name) {
    return GetRegisterOfTheSymbol(name) != -1 || GetOffsetOfTheSymbol(name) != (uint64_t)-1;
}

// allocate a reg for a new symbol
int AllocateRegisterForTheSymbol(const char *name, bool is_string, const char *string_value) {
    // check if alr allocated
    int existing = GetRegisterOfTheSymbol(name);
    if(existing != -1) {
        return existing;
    }
    
    // check limits
    if(symbol_count >= MAX_SYMBOLS) {
        return -1;
    }
    
    // skip r1-r4 (r4 is for syscall args)
    // skip forward to r5 if we're in the syscall range
    if(next_reg >= 1 && next_reg <= 4)
        next_reg = 5;

    
    if(next_reg > REG_MAX)
        return -1;
    
    // add symbol to table
    strncpy(table[symbol_count].name, name, MAX_NAME_LEN - 1);
    table[symbol_count].name[MAX_NAME_LEN - 1] = '\0';
    table[symbol_count].reg = next_reg;
    table[symbol_count].offset = next_offset;
    table[symbol_count].is_string = is_string;  // FIX 24
    
    // FIX 24
    if(is_string && string_value) {
    table[symbol_count].string_value = strdup(string_value);
    table[symbol_count].string_len = strlen(string_value) + 1; // +1 for null
    // For strings, offset increments by actual string length
    next_offset += table[symbol_count].string_len;
    // Align to 8 bytes for next variable
    if(next_offset % 8 != 0) {
        next_offset += 8 - (next_offset % 8);
    }
    } else {
        table[symbol_count].string_value = NULL;
        table[symbol_count].string_len = 0;
        // For non-strings, standard 8 bytes
        next_offset += 8;
    }

    symbol_count++;
    next_offset += 8;  // 8 bytes/variable
    
    return next_reg++;
}

// FIX: 1555555
// add label (for strings) w/o register
// this is teh key fix for "daddiu r4, r0, str0" prob
// previously string labels (str0, str1, ...) were not in the symbol table
// so GetOffsetOfTheSymbol("str0") returned -1 -> machine code generator failed
// now we add them here w/ reg = -1 & proper offset
// size includes null terminator (strlen(value) + 1)
void AddLabel(const char *name, uint64_t size) {
    // check if alr exists (avoid duplicates)
    for(int i = 0; i < symbol_count; i++) {
        if(strcmp(table[i].name, name) == 0) {
            return;
        }
    }
    
    if(symbol_count >= MAX_SYMBOLS) {
        return;
    }
    
    strncpy(table[symbol_count].name, name, MAX_NAME_LEN - 1);
    table[symbol_count].name[MAX_NAME_LEN - 1] = '\0';
    table[symbol_count].reg = -1;           // marks this as a label, not a variable
    table[symbol_count].offset = next_offset;
    
    symbol_count++;
    next_offset += size;  // advance offset by string size (including '\0')
}

// get memory offset for symbol
// works for both variables & string labels (str0, str1, ...)
// this is what the machine code generator uses to resolve "daddiu r4, r0, str0"
uint64_t GetOffsetOfTheSymbol(const char *name) {
    for(int i = 0; i < symbol_count; i++) {
        if(strcmp(table[i].name, name) == 0) {
            return table[i].offset;
        }
    }
    return (uint64_t)-1; // not found
}

// print symbol table for debugging
// only shows vars (reg != -1), str labels are omitted for clarity
// but they r still in teh table & accessible via GetOffsetOfTheSymbol
void PrintAllSymbols(FILE *out) {
    fprintf(out, "; Symbol Table\n");
    fprintf(out, "; Name\tReg\tOffset\n");
    for(int i = 0; i < symbol_count; i++) {
        if(table[i].reg != -1) {
            fprintf(out, "; %s\tr%d\t0x%lX\n",
                    table[i].name,
                    table[i].reg,
                    (unsigned long)table[i].offset);
        }
    }
    fprintf(out, "\n");
}