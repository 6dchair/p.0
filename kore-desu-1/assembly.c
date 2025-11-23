#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "assembly.h"
#include "symbol_table.h"
#include "usage_counter.h"

// temporary registers for expression evaluation (r20–r30)
// used for intermediate values in expressions
static int temp_start = 20;
static int temp_next = 20;
static int temp_max = 30;

// reset temp register usage and called b4 generating code
void AssemblyInit() {
    temp_next = temp_start;
}

// allocate a temp register for intermediate computation results
// automatically goes back to 20 if exceeded
static int NewTempRegister() {
    int r = temp_next++;
    if(temp_next > temp_max) temp_next = temp_start;
    return r;
}

// reset the temp reg pointer after each statement
// to prevent register reuse conflicts in multiple lines
static void ResetTempRegister() {
    temp_next = temp_start;
}

//  (forward declrations) recursive expression parsing
static int Parse_E(const char **p, FILE *out, int target);
static int Parse_T(const char **p, FILE *out, int target);
static int Parse_F(const char **p, FILE *out, int target);
static void SkipSpacesPtr(const char **p);

// load var: generates mips64 insruction to load a var's value into a register
static void LoadVariable(FILE *out, int reg, const char *name) {
    fprintf(out, "ld r%d, %s(r0)\n", reg, name);
}

// store: generate instruction to store a reg's value into memory
static void StoreVariable(FILE *out, int reg, const char *name) {
    fprintf(out, "sd r%d, %s(r0)\n", reg, name);
}

// load immediate constant into a register
static void GenerateLoadImmediate(FILE *out, int reg, long long imm) {
    if(imm > 15) 
        fprintf(out, "daddiu r%d, r0, #0x%llX\n", reg, imm);
    else if(imm < -15)
        fprintf(out, "daddiu r%d, r0, #-%#llX\n", reg, -imm);
    else
        fprintf(out, "daddiu r%d, r0, #%lld\n", reg, imm);
}

// Generate binary arithmetic instructions (+, -, *, /)
static void GenerateBinOp(FILE *out, const char *op_mnemonic, int dst, int r1, int r2) {
    if(strcmp(op_mnemonic, "dmul") == 0) {
        // Multiply r1 * r2, result in LO
        fprintf(out, "dmult r%d, r%d\n", r1, r2);
        fprintf(out, "mflo r%d\n", dst);  // move LO directly to dst
    } else if(strcmp(op_mnemonic, "ddiv") == 0) {
        // Divide r1 / r2, quotient in LO
        fprintf(out, "ddiv r%d, r%d\n", r1, r2);
        fprintf(out, "mflo r%d\n", dst);  // move LO directly to dst
    } else {
        // Standard arithmetic
        fprintf(out, "%s r%d, r%d, r%d\n", op_mnemonic, dst, r1, r2);
    }
}


// skip spaces to advance pointer past whitespaces in the expression
static void SkipSpacesPtr(const char **p) {
    while(**p && isspace(**p))
        (*p)++;
}

// parse F (factor): lowest precedence
// F -> (E) | vars | numbers
// returns register number containing result
static int Parse_F(const char **p, FILE *out, int target) {
    SkipSpacesPtr(p);

    // (): recursively parse inner expression
    if(**p == '(') {
        (*p)++;
        int r = Parse_E(p, out, target);
        SkipSpacesPtr(p);
        if(**p == ')')
            (*p)++;
        return r;
    }

    // mumeric literal
    if(isdigit(**p)) {
        long long val = 0;
        while(**p && isdigit(**p)) { 
            val = val*10 + (**p-'0'); 
            (*p)++; 
        }
        int r = target ? target : NewTempRegister();
        GenerateLoadImmediate(out, r, val);
        return r;
    }

    // variable
    if(isalpha(**p) || **p == '_') {
        char name[128]; int i=0;
        while(**p && (isalnum(**p)||**p=='_')) 
            if(i<127)
                name[i++] = *(*p)++;
        name[i]='\0';
        int reg = GetRegisterOfTheSymbol(name);
        if(reg==-1) 
            reg = AllocateRegisterForTheSymbol(name);
        LoadVariable(out, reg, name);
        return reg;
    }

    return 0;
}

// parse T (term): handles * and / ops
// left-associative chaining
// T -> T * F | T / F | F
static int Parse_T(const char **p, FILE *out, int target) {
    int left = Parse_F(p, out, 0);
    while(1) {
        SkipSpacesPtr(p);
        if(**p=='*'||**p=='/') {
            char op = **p; (*p)++;
            SkipSpacesPtr(p);
            int right = Parse_F(p, out, 0);
            int dst = target ? target : NewTempRegister();
            if(op=='*') 
                GenerateBinOp(out,"dmul",dst,left,right);
            else 
                GenerateBinOp(out,"ddiv",dst,left,right);
            left = dst;
        } else 
            break;
    }
    return left;
}

// parse E (expression)
// Handles + and -; also left-associative
// E -> E + T | E - T | T
static int Parse_E(const char **p, FILE *out, int target) {
    int left = Parse_T(p, out, target);
    while(1) {
        SkipSpacesPtr(p);
        if(**p=='+'||**p=='-') {
            char op = **p; (*p)++;
            SkipSpacesPtr(p);
            int right = Parse_T(p, out, 0);
            int dst = NewTempRegister();
            if(op=='+') 
                GenerateBinOp(out,"daddu",dst,left,right);
            else 
                GenerateBinOp(out,"dsubu",dst,left,right);
            left = dst;
        } else 
            break;
    }
    return left;
}

// assembly for declaration
// alloacate register, parse RHS if present
// generate store instruction to memory
int AssemblyGenerateDeclaration(const Statement *stmt, FILE *out) {
    if(!stmt || stmt->type != STMT_DECL)
        return 0;

    int reg = AllocateRegisterForTheSymbol(stmt->lhs);
    if(reg == -1) 
        return 0;

    // Only generate code if RHS is non-empty
    if(stmt->rhs[0] != '\0') {
        const char *p = stmt->rhs;
        int rres = Parse_E(&p, out, reg);
        if(rres != reg)
            fprintf(out,"daddu r%d,r%d,r0\n",reg,rres);
        StoreVariable(out, reg, stmt->lhs);
    }
    return 1;
}

// asse,bly for assignment
// parse expression and store to variable
int AssemblyGenerateAssignment(const Statement *stmt, FILE *out) {
    if(!stmt || stmt->type != STMT_ASSIGN)
        return 0;

    int lhs_reg = GetRegisterOfTheSymbol(stmt->lhs);
    if(lhs_reg == -1)
        lhs_reg = AllocateRegisterForTheSymbol(stmt->lhs);

    const char *rhs = stmt->rhs;
    char *endptr;
    long imm_val = strtol(rhs, &endptr, 0);

    if(*endptr == '\0') {
        // directly assign literals into permanent registers r1–r19
        GenerateLoadImmediate(out, lhs_reg, imm_val);
    } else {
        int rres = Parse_E(&rhs, out, lhs_reg);
        if(rres != lhs_reg)
            fprintf(out,"daddu r%d,r%d,r0\n", lhs_reg, rres);
    }

    StoreVariable(out, lhs_reg, stmt->lhs);
    return 1;
}

// single statement
// dispatch each parsed statement to the correct generator
// reset temp regs between statements to avoid overlap
int GenerateAssemblyStatement(const Statement *stmt, FILE *out) {
    if(!stmt||!out)
        return 0;
    ResetTempRegister();
    if(stmt->type==STMT_DECL)
        return AssemblyGenerateDeclaration(stmt,out);
    if(stmt->type==STMT_ASSIGN) 
        return AssemblyGenerateAssignment(stmt,out);
    return 0;
}

// Full program
// make entry point for code generation
// a. iniialize symbol table
// b. generate .data section w/ var declarations
// c. generate .code section 
void AssemblyGenerateProgram(const Statement *stmts,int count,FILE *out){
    SymbolInit();
    fprintf(out,".data\n");
    // only declare variables, no duplicates, no zero init
    for(int i=0;i<count;i++)
        if(stmts[i].type==STMT_DECL)
            fprintf(out,"%s: .space 8\n",stmts[i].lhs);

    fprintf(out,"\n.code\n");
    for(int i=0;i<count;i++)
        GenerateAssemblyStatement(&stmts[i],out);
}

