#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "assembly.h"
#include "symbol_table.h"
#include "ast.h"

/////
// string table structure
typedef struct {
    char *label;
    char *value;
} StringEntry;

// string table variables
static StringEntry string_table[100];
static int string_count = 0;
static int string_label_counter = 0;
/////

// rack w/c variables have been explicitly initialized
static char *initialized_vars[100];
static int init_var_count = 0;

// temporary registers for expression evaluation (r20–r30)
static int temp_start = 20;
static int temp_next = 20;
static int temp_max = 30;

static char* GetStringLabel(const char *str) {
    // process escape sequences in the input string
    char *processed_str = malloc(strlen(str) * 2 + 1);
    char *dst = processed_str;
    
    for(const char *src = str; *src; src++) {
        if(*src == '\\' && *(src+1)) {
            // hndle escape sequence
            src++;
            switch(*src) {
                case 'n': *dst++ = '\n'; break;
                case 't': *dst++ = '\t'; break;
                case '"': *dst++ = '"'; break;
                case '\\': *dst++ = '\\'; break;
                default:
                    // invalid, keep both chars
                    *dst++ = '\\';
                    *dst++ = *src;
                    break;
            }
        } else {
            *dst++ = *src;
        }
    }
    *dst = '\0';
    
    // now check if this processed string already exists
    for(int i = 0; i < string_count; i++) {
        if(strcmp(string_table[i].value, processed_str) == 0) {
            free(processed_str);
            return string_table[i].label;
        }
    }
    
    // store the processed string (with actual newlines, not \n)
    if(string_count >= 100) {
        free(processed_str);
        return NULL;
    }
    
    string_table[string_count].value = processed_str;  // store as-is
    
    // generate label
    char *label = malloc(20);
    sprintf(label, "str%d", string_label_counter++);
    string_table[string_count].label = label;
    
    string_count++;
    return label;
}

static void mark_initialized(const char *name) {
    for(int i = 0; i < init_var_count; i++) {
        if(strcmp(initialized_vars[i], name) == 0)
            return;
    }
    if(init_var_count < 100) {
        initialized_vars[init_var_count++] = strdup(name);
    }
}

// reset temp register usage
void AssemblyInit() {
    temp_next = temp_start;
    init_var_count = 0;
}

// allocate a temp register for intermediate computation
static int NewTempRegister() {
    int r = temp_next++;
    if(temp_next > temp_max)
        temp_next = temp_start;
    return r;
}

// reset the temp reg pointer after each statement
static void ResetTempRegister() {
    temp_next = temp_start;
}

// load variable: generates MIPS64 instruction to load a var's value
static void LoadVariable(FILE *out, int reg, const char *name) {
    fprintf(out, "ld r%d, %s(r0)\n", reg, name);
}

// store: generate instruction to store a reg's value into memory
static void StoreVariable(FILE *out, int reg, const char *name) {
    fprintf(out, "sd r%d, %s(r0)\n", reg, name);
}

// load immediate constant into a register
static void GenerateLoadImmediate(FILE *out, int reg, long long imm) {
    fprintf(out, "daddiu r%d, r0, #%lld\n", reg, imm);
}

// generate binary arithmetic instructions
static void GenerateBinOp(FILE *out, const char *op_mnemonic, int dst, int r1, int r2) {
    if(strcmp(op_mnemonic, "dmul") == 0) {
        fprintf(out, "dmult r%d, r%d\n", r1, r2);
        fprintf(out, "mflo r%d\n", dst);
    } else if(strcmp(op_mnemonic, "ddiv") == 0) {
        fprintf(out, "ddiv r%d, r%d\n", r1, r2);
        fprintf(out, "mflo r%d\n", dst);
    } else {
        fprintf(out, "%s r%d, r%d, r%d\n", op_mnemonic, dst, r1, r2);
    }
}

// helper function to collect symbols from AST
static void CollectSymbolsFromAST(Node *node) {
    if(!node)
        return;
    
    // traverse linked list of statements
    Node *current = node;
    while(current) {
        switch(current->node_type) {
            ////
            case 1: // NODE_STR - string literal
                GetStringLabel(current->str_val);
                break;
            /////

            case 4: { // NODE_DECL - declaration
                Node *item = current->list.items;
                while(item) {
                    if(item->node_type == 2) {
                        // simple declaration: int x
                        AllocateRegisterForTheSymbol(item->str_val);
                    } else if(item->node_type == 3 && item->binop.op == '=') {
                        // initialized declaration: int x = expr
                        if(item->binop.left && item->binop.left->node_type == 2) {
                            AllocateRegisterForTheSymbol(item->binop.left->str_val);
                        }
                        // also collect from expression
                        CollectSymbolsFromAST(item->binop.right);
                    }
                    item = item->list.next;
                }
                break;
            }
                
            case 5: { // NODE_ASSIGN - assignment
                Node *assign = current->list.items;
                while(assign) {
                    if(assign->node_type == 3 && assign->binop.op == '=') {
                        if(assign->binop.left && assign->binop.left->node_type == 2) {
                            AllocateRegisterForTheSymbol(assign->binop.left->str_val);
                        }
                        // collect from expression
                        CollectSymbolsFromAST(assign->binop.right);
                    }
                    assign = assign->list.next;
                }
                break;
            }
                
            case 6: { // NODE_PRINT - print statement
                Node *part = current->print_stmt.parts;
                while(part) {
                    if(part->node_type == 7) {  // NODE_PRINT_PART
                        Node *content = part->list.items;
                        if(content && content->node_type == 1) {  // NODE_STR
                            GetStringLabel(content->str_val);
                        } else {
                            CollectSymbolsFromAST(content);
                        }
                    } else if(part->node_type == 1) {  // NODE_STR
                        GetStringLabel(part->str_val);
                    } else {
                        CollectSymbolsFromAST(part);
                    }
                    part = part->list.next;
                }
                break;
            }
            /////
                
            case 3: // NODE_BINOP - expression
                CollectSymbolsFromAST(current->binop.left);
                CollectSymbolsFromAST(current->binop.right);
                break;
                
            case 2: // NODE_ID - variable reference
                AllocateRegisterForTheSymbol(current->str_val);
                break;
            
            /////
            case 7: // NODE_PRINT_PART
                CollectSymbolsFromAST(current->list.items);
                break;
            ////
        }
        
        // move to next statement
        current = current->list.next;
    }
}

static int GenerateExpression(Node *node, FILE *out, int target_reg) {
    if(!node)
        return 0;
    
    // hanndle NODE_PRINT_PART wrapper
    if(node->node_type == 7) {  // NODE_PRINT_PART
        return GenerateExpression(node->list.items, out, target_reg);
    }

    switch(node->node_type) {
        case 0: { // NODE_NUM
            int reg = target_reg ? target_reg : NewTempRegister();
            GenerateLoadImmediate(out, reg, node->int_val);
            return reg;
        }
        case 2: { // NODE_ID (variable)
            int var_reg = GetRegisterOfTheSymbol(node->str_val);
            if(var_reg == -1)
                var_reg = AllocateRegisterForTheSymbol(node->str_val);
            
            // always load from memory
            if(target_reg) {
                // load directly into target register
                fprintf(out, "ld r%d, %s(r0)\n", target_reg, node->str_val);
                return target_reg;
            } else {
                // load into variable's own register
                fprintf(out, "ld r%d, %s(r0)\n", var_reg, node->str_val);
                return var_reg;
            }
        }

        
        case 3: { // NODE_BINOP
            // for assignment, handle separately
            if(node->binop.op == '=')
                return GenerateExpression(node->binop.left, out, target_reg);
            
            // evaluate both sides
            int left_reg = GenerateExpression(node->binop.left, out, 0);
            int right_reg = GenerateExpression(node->binop.right, out, 0);
            
            // for multiplication, need to handle mflo
            if(node->binop.op == '*') {
                // generate multiplication
                fprintf(out, "dmult r%d, r%d\n", left_reg, right_reg);
                
                // get result register
                int result_reg = target_reg ? target_reg : NewTempRegister();
                fprintf(out, "mflo r%d\n", result_reg);
                return result_reg;
            }
            
            // for other operations, use target reg if there is
            int result_reg = target_reg ? target_reg : NewTempRegister();
            
            switch(node->binop.op) {
                case '+':
                    GenerateBinOp(out, "daddu", result_reg, left_reg, right_reg);
                    break;
                case '-':
                    GenerateBinOp(out, "dsubu", result_reg, left_reg, right_reg);
                    break;
                case '/':
                    GenerateBinOp(out, "ddiv", result_reg, left_reg, right_reg);
                    break;
            }
            
            return result_reg;
        }
        
        default:
            return 0;
    }
}

// generate assembly for declaration
static void GenerateDeclaration(Node *node, FILE *out) {
    if(!node || node->node_type != 4)
        return;
    
    Node *current = node->list.items;
    while(current) {
        if(current->node_type == 3 && current->binop.op == '=') {
            // dwclaration with initialization: int x = expr
            Node *left = current->binop.left;
            Node *right = current->binop.right;
            
            // allocate register for variable
            int reg = AllocateRegisterForTheSymbol(left->str_val);
            if(reg == -1)
                return;
            
            mark_initialized(left->str_val);
            
            // generate code for expression
            // int expr_reg = GenerateExpression(right, out);
            int expr_reg = GenerateExpression(right, out, reg);  // pass target register
            
            // store result to variable
            if(expr_reg != reg) {
                fprintf(out, "daddu r%d, r%d, r0\n", reg, expr_reg);
            }
            StoreVariable(out, reg, left->str_val);
            
        } else if(current->node_type == 2) {
            // declaration without initialization: int x
            int reg = AllocateRegisterForTheSymbol(current->str_val);
            if(reg == -1)
                return;
            
            // don't initialize to 0 - just allocate
        }
        current = current->list.next;
    }
}

// generate assembly for assignment
static void GenerateAssignment(Node *node, FILE *out) {
    if(!node || node->node_type != 5) 
        return;
    
    Node *current = node->list.items;
    while(current) {
        if(current->node_type == 3 && current->binop.op == '=') {
            Node *left = current->binop.left;
            Node *right = current->binop.right;
            
            // get or allocate register for left variable
            int left_reg = GetRegisterOfTheSymbol(left->str_val);
            if(left_reg == -1) {
                left_reg = AllocateRegisterForTheSymbol(left->str_val);
            }
            
            mark_initialized(left->str_val);
            
            // ALWAYS use GenerateExpression to get target register optimization
            int expr_reg = GenerateExpression(right, out, left_reg);
            
            // store result to memory
            // expr_reg should be left_reg if target register was used
            StoreVariable(out, expr_reg, left->str_val);
            
            // no need for daddu - GenerateExpression should have loaded directly
            // into left_reg if it was a simple variable
        }
        current = current->list.next;
    }
}

// generate assembly for print statement - eduMIPS64 version
static void GeneratePrint(Node *node, FILE *out) {
    if(!node || node->node_type != 6)
        return;
    
    Node *current = node->print_stmt.parts;
    while(current) {
        Node *content = current;
        
        // handle NODE_PRINT_PART wrapper
        if(current->node_type == 7) {  // NODE_PRINT_PART
            content = current->list.items;
        }
        
        if(content->node_type == 1) {  // NODE_STR (string)
            // get the label for this string
            char *label = GetStringLabel(content->str_val);
            if(label) {
                // eduMIPS64: load string address into r1, syscall 4 for string print
                fprintf(out, "daddi r1, r0, %s\n", label);  // load string address
                fprintf(out, "syscall 4\n");               // print string
            }
        } else {
            // integer expression
            int reg = GenerateExpression(content, out, 0);
            
            // eduMIPS64 print integer: value in r1, syscall 1
            if(reg != 1) {  // if value not already in r1
                fprintf(out, "dadd r1, r%d, r0\n", reg);  // move to r1
            }
            fprintf(out, "syscall 1\n");  // print integer
            
            // optional: print space between items (remove if not needed)
            fprintf(out, "daddi r1, r0, #32\n");  // ASCII space
            fprintf(out, "syscall 11\n");         // print character
        }
        current = current->list.next;
    }
    
    // print newline after print statement
    fprintf(out, "daddi r1, r0, #10\n");  // ASCII newline
    fprintf(out, "syscall 11\n");         // print character
}
///////////////


// generate assembly for a single AST node
void GenerateAssemblyNode(Node *node, FILE *out) {
    if(!node || !out)
        return;
    
    ResetTempRegister();
    
    switch(node->node_type) {
        case 4: // NODE_DECL
            GenerateDeclaration(node, out);
            break;
        case 5: // NODE_ASSIGN
            GenerateAssignment(node, out);
            break;
        case 6: // NODE_PRINT
            GeneratePrint(node, out);
            break;
        default:
            // list node - traverse
            if(node->list.items) {
                GenerateAssemblyNode(node->list.items, out);
            }
            if(node->list.next) {
                GenerateAssemblyNode(node->list.next, out);
            }
            break;
    }
}

// full program generation
void GenerateAssemblyProgram(Node *program, FILE *out) {
    if(!program || !out)
        return;
    
    // initialize
    SymbolInit();
    AssemblyInit();
    string_count = 0;           // reset string table
    string_label_counter = 0;   // reset label counter
    
    // first, process the AST to collect all symbols AND strings
    CollectSymbolsFromAST(program);
    
    // generate .data section with all variables AND strings
    fprintf(out, ".data\n");
    PrintDataSection(out);  // this prints .space for each variable
 
    // Generate string data
    for(int i = 0; i < string_count; i++) {
        fprintf(out, "%s: .asciiz \"", string_table[i].label);
        
        // Write string character by character, escaping as needed
        for(char *p = string_table[i].value; *p; p++) {
            if(*p == '\n') {
                fprintf(out, "\\n");
            } else if(*p == '"') {
                fprintf(out, "\\\"");
            } else if(*p == '\\') {
                fprintf(out, "\\\\");
            } else {
                fputc(*p, out);
            }
        }
        
        fprintf(out, "\"\n");
    }
    fprintf(out, "\n.code\n");

    // generate code - traverse the linked list of statements
    Node *current = program;
    while(current) {
        GenerateAssemblyNode(current, out);
        current = current->list.next;
    }
    
    // clean up string table
    for(int i = 0; i < string_count; i++) {
        free(string_table[i].value);
        free(string_table[i].label);
    }
}

////////////