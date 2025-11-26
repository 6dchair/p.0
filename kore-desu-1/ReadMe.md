# Source -> MIPS64 Assemby -> Machine code (Hardcoded using C language)

### This project reads a source text file written in a simplified high-level language (adhering to C syntax) and generates: (1) MIPS64 assembly code saved to a .txt file, and (2) Machine code in both binary and hex formats saved to an .mc file.
### And displayed on the terminal are the lines and whether each line is valid or erroneous (displays error type or its cause).

## This project consists of the ff C source and header files:

    1.Line validator
        - checks syntax validity of each line (no assembly, no machine code)
        - decects errors such as invalid identifier, undeclared/redeclared vars, missing semicplon, and invalid expression or syntax in general
        - tracks declared variables using a separate internal list: vars[][]
        - outputs the first error for the line
        - prodces valid/invalid feedback before any parsing or assembly happens
    2. Parser: 
        - reads each approved line by the line_validator
        - converts it into one or more Statement structures (fields: statement type, LHS, RHS, raw (full))
        - labels each statement as STMT_DECL (declaration), or STMT_ASSIGN (assignment)
        - store the RHS as plain text
    3. Assembly code generator: 
        - converts parsed statemnets into full MIPS64 assembly instructions
        - automatically produces two sections: .data & .code
        - for declarations (e.g., int x = 5;):
            * calls AllocateRegisterForTheSymbol() to give the LHS var a permanent register
            * emits memory allocation directives (x: .space 8)
            * if RHS exists:
                - recursively parse the expr
                - emits arithmetic instructions (daddiu, etc.)
                - stores the final value to memory using sd
            * generates ld, sd, daddiu, dmult, ddiv, etc.
        - for assignments (e.g., x = y + 3;):
            * ensures LHS has a permanent reg
            * checks whether RHS is a pure literal:
                - if yes: directly emits immediate load (daddiu)
                - else: recursively parses and eva;uates the expr
            * loads source operands (ld)
            * generates arithmetic instructions (daddu, dsubu, dmult, ddiv)
            * stores final result back to memory (sd)
        - uses a temporary register pool (r20–r30)
            * parsing functions allocate temps using NewTempRegister()
            * temp regs are used only for imm arithmetic results
        - delegates all variable2register mappinf to the symbol table module
    4. Machine code generator: 
        - reads the assembly file linexline
        - uses pattern matching (sscanf) to detect instruction formats
        - converts each MIPS64 instruction into binary machine code & hex representation
        - uses the symbol table to convert var names into memory offsets (for sd & ld)
        - writes the machine code into .mc output file
    5. Error handler:
        - defines error types (syntax, redeclared, missing semicolon, invalid expression, undeclared variable, & invalid expression or syntax in general)
        - integrated with line_validator.c to report the first encountered error
        - main stops compilation immediately upon any error
    6. Symbol table:
        - maintains the global list of declared vars
        - maps each variable to:
            * a dedicted register
            * a memory offset in the .data segment
        - provides functionality:
            a. AllocateRegisterForTheSymbol()
            b. GetRegisterOfTheSymbol()
            c. GetOffsetOfTheSymbol()
            d. PrintAll() // commented; for debugging purposes
        - ensures consistent allocation between assembly statments
        - reset table via SymbolInit()
    7. Main file: 
        - controls the entire compilation pipeline:
            a. opens the input file
            b. reads lines one by one
            c. removes whitespace
            d. sends each line to the validator
            e. prints the line and validation result
            f. stops immediately on the first error
            g. if all lines are valid:
                - parses them into Statement structures
                - generates MIPS64 assembly program
                - generates machine code file
        - ensures no assembly or machine code is produced when errors occur

# Flow:
    1. Read the source file line by line
    2. Check for errors in the line
        2.1 Program ends when an error is encountered
    3. Parse the valid line into a standardized statement structure (statement type, LHS, RHS, raw (full))
    4. Close the source file
    5. Analyze variable usage for register allocation
    6. Generate assembly code from the parsed statement structures
    7. Generate the machine code using the generated assembly code text file
    8. End program execution