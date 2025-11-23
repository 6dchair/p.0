#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "line_validator.h" // syntax validation functions (per line)
#include "parser.h" // parsing input lines into Statement structs
#include "assembly.h" // assembly code generation from parsed statements
#include "symbol_table.h" // variable-to-register mapping management
#include "machine_code.h"  // conversion of assembly to machine code
#include "usage_counter.h" // register priority handling

int main(void) {
    // 1) OPEN SOURCE FILE
    FILE *f = fopen("INPUT.txt", "r"); 
    if(!f) {
        printf("Unable to access the input text file\n");
        return 1;                        
    }

    // 2) INITIAL SETUP
    char buffer[BUFFER]; // stores each line read from source file; BUFFER defined in line_validator.c
    Statement stmts[1024];  // global storage for all parsed statements from the entire text file
    int stmt_count = 0; // total count of valid parsed statements or keeps track of how many valid statements have been stored

    SymbolInit(); // initialize the symbol table before parsing

    // display header for readability in terminal
    printf("****** SOURCE->MIPS64->MACHINE CODE ******\n");

    int buffer_count = 1;
    int error_found = 0;   // error flag to stop output generation

    // 3) READ FILE LINE BY LINE
    while(fgets(buffer, sizeof(buffer), f)) {
        RemoveLeadingAndTrailingSpaces(buffer); // trim leading/trailing spaces
        if(buffer[0] == '\0')
            continue; // skip blank lines

        printf("[Line %d]: ", buffer_count++);
        //int isbuffervalid = 0; // flag for syntax validation result
        char errinfo[MAX_VAR_LENGTH];  // buffer for error info
        ErrorType err;

        // 3A) DETERMINE LINE TYPE
        if(strncmp(buffer, "int ", 4) == 0)
            err = StartsWithInt(buffer, errinfo);
        else
            err = StartsWithVariableName(buffer, errinfo);

        // 3B) HANDLE INVALID LINES
        if(err != ERR_NONE) {
            printf("%s\n", buffer);
            ReportError(err, buffer_count - 1, errinfo);
            error_found = 1;
            // continue; // skip to next line
            // or
            goto endeu;
            return 1;
        }

        // 3C) VALID LINE HANDLING
        printf("%s\n\tTransform: Correct syntax\n\n", buffer);

        // parse valid line into Statement structures
        Statement parsed[32];
        int parsed_count = ParseStatement(buffer, parsed);

        // store parsed statements (but do NOT generate assembly yet)
        for(int k = 0; k < parsed_count; k++) {
            if(stmt_count < (int)(sizeof(stmts)/sizeof(stmts[0])))
                stmts[stmt_count++] = parsed[k];
        }
    }

    fclose(f); // close source file

    // abort if any syntax error found
    if(error_found) {
        endeu:
        printf("Compilation aborted due to encountered invalid syntax. No assembly and machine codes generated.\n\n");
        return 1;
    }

    // 8): FINAL OUTPUT FILES
    // before generating the final assembly, analyze variable usage frequency
    AnalyzeVariableUsage(stmts, stmt_count);

    // generate full MIPS64 assembly program
    FILE *MIPS64_ASSEMBLY = fopen("MIPS64_ASSEMBLY.txt", "w");
    if(!MIPS64_ASSEMBLY) {
        printf("Cannot create file\n");
        return 1;
    }
    AssemblyGenerateProgram(stmts, stmt_count, MIPS64_ASSEMBLY);
    fclose(MIPS64_ASSEMBLY);

    // generate final machine code (based on completed assembly)
    FILE *MACHINE_CODE = fopen("MACHINE_CODE.mc", "w");
    if(!MACHINE_CODE) {
        printf("Cannot create machine code output file\n");
        return 1;
    }

    // convert the full assembly to machine code
    MachineFromAssembly("MIPS64_ASSEMBLY.txt", "MACHINE_CODE.mc");
    fclose(MACHINE_CODE);

    printf("Compilation successful. Assembly and machine codes generated.\n\n");
}
