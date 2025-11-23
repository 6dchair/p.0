# Source -> MIPS64 Assemby -> Machine code (Hardcoded using C language)

# This project reads a source text file written in a simplified high-level language (adhering to C syntax) and generates: (1) MIPS64 assembly code saved to a .txt file, and (2) Machine code in both binary and hex formats saved to an .mc file.
# And displayed on the terminal are the lines and whether each line is valid or erroneous (displays error type or its cause).

# This project consists of the ff C source and header files:

    1. line validator file: checks syntax validity of each line
    2. a parser: parses diff statements from each line into statement structures; also labels each line as a declaration, assignment, or invalid
    3. assembly code generator: converts the parsed statements into MIPS64 assembly code
    4. machine code generator: encodes the generated assembly instructions into actual machine code (binary + hex)
    5. register usage manager: tracks variable usage frequency and allocates permanent registers to the most frequently use variables
    6. error handler: defines possible error types and labels each line the first error type encountered
    7. symbol table: stores and maps variables to registers and manages variables?register pair
    8. and a main file: coordinates the flow to process each line of input by utilizing the different files, prints validation details, outputs the lines in the terminal and whether it's valid or not (includes the reason for error or error type). The program stops execution on the first error encounter and no assembly and machine code generated.

# General workflow:
    1. Read the source file line by line
    2. Check for errors in the line
        2.1 Program ends when an error is encountered
    3. Parse the valid line into a standardized statement structure (statement type, LHS, RHS, raw (full))
    4. Close the source file
    5. Analyze variable usage for register allocation
    6. Generate assembly code from the parsed statement structures
    7. Generate the machine code using the generated assembly code text file
    8. End program execution