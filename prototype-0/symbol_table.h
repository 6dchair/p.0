#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <stdio.h>
#include <stdint.h>

#define MAX_SYMBOLS 100
#define MAX_NAME_LEN 32
#define REG_MIN 1
#define REG_MAX 31

void SymbolInit();
int GetRegisterOfTheSymbol(const char *name);
int SymbolExists(const char *name);
int AllocateRegisterForTheSymbol(const char *name);
uint64_t GetOffsetOfTheSymbol(const char *name);
void PrintAllSymbols(FILE *out);
void PrintDataSection(FILE *out);

#endif