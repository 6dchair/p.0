%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "semantics.h"
#include "ast.h"
#include "assembly.h"
#include "machine_code.h"
#include "interpreter.h"

#define NODE_PRINT_PART 7

// AST root
Node *ast_root = NULL;

// global semantic analyzer
Semantics sem_analyzer;

extern int yylex();
extern int yyparse();
extern FILE *yyin;
void yyerror(const char *s);
int yylex_destroy(void);

Node *create_num_node(int val);
Node *create_str_node(char *str);
Node *create_id_node(char *name);
Node *create_binop_node(int op, Node *left, Node *right);
Node *create_decl_node(Node *items);
Node *create_assign_node(Node *items);
Node *create_print_node(Node *parts);
Node *append_to_list(Node *list, Node *item);
Node *create_print_part_node(Node *content);  // ADD THIS LINE
void free_node(Node *node);

// debug function
void print_ast(Node *node, int depth);

%}

%union {
    int int_val;
    char *str_val;
    void *node_ptr;
}

%token PROG_START PROG_END
%token KW_INT KW_PRINT
%token NEWLINE_TOKEN ILLEGAL
%token <int_val> NUM
%token <str_val> ID STR
%token ILLEGAL

%type <node_ptr> program lines line full_line
%type <node_ptr> decl print_stmt assign
%type <node_ptr> decl_items more_decl_items decl_item
%type <node_ptr> more_assign
%type <node_ptr> print_parts more_print_parts print_part
%type <node_ptr> expr term factor

//%left '+' '-'
//%left '*' '/'
%right '+' '-'
%left '*' '/'
%nonassoc PRINT_EXPR
%left '(' ')'

%%

program: PROG_START lines PROG_END
    {
        ast_root = $2;
        //printf("Parsed program successfully\n");
    }
    ;

lines: line lines
   {
        $$ = append_to_list((Node*)$1, (Node*)$2);
    }
    | /* epsilon */
    {
        $$ = NULL;
    }
    ;

line: full_line NEWLINE_TOKEN
    {
        $$ = $1;
        sem_set_line(&sem_analyzer, sem_analyzer.current_line + 1);
    }
    | error NEWLINE_TOKEN
    {
        // skip erroneous line
        //fprintf(stderr, "Syntax error at line %d, skipping line\n", sem_analyzer.current_line);
        $$ = NULL;
        sem_set_line(&sem_analyzer, sem_analyzer.current_line + 1);
        yyerrok;  // reset error recovery
    }
    | NEWLINE_TOKEN
    {
        $$ = NULL;
        sem_set_line(&sem_analyzer, sem_analyzer.current_line + 1);
    }
    ;

full_line: decl
    {
        $$ = $1;
        sem_set_decl_line(&sem_analyzer, false);  // reset after declaration line
    }
    | print_stmt
    {
        $$ = $1;
    }
    | assign
    {
        $$ = $1;
    }
    ;

decl: KW_INT { sem_set_decl_line(&sem_analyzer, true); } decl_items
    {
        sem_set_decl_line(&sem_analyzer, false);  // reset after declaration
        $$ = create_decl_node((Node*)$3);
    }
    ;

decl_items: decl_item more_decl_items
    {
        if($1) {
            $$ = append_to_list((Node*)$1, (Node*)$2);
        } else {
            $$ = $2;  // skip this item, continue with rest
        }
    }
    ;

more_decl_items: ',' decl_item more_decl_items
    {
        if($2) {
            $$ = append_to_list((Node*)$2, (Node*)$3);
        } else {
            $$ = $3;  // Skip this item, continue with rest
        }
    }
    | /* epsilon */
    {
        $$ = NULL;
    }
    ;

decl_item: ID
    {
        // in declaration line: just add symbol
        if(sem_add_symbol(&sem_analyzer, $1)) {
            $$ = create_id_node($1);
        } else {
            $$ = NULL;  // Error occurred (e.g., duplicate declaration)
        }
    }
    | ID '=' expr
    {
        // in declaration line: add symbol and create initialization
        if(sem_add_symbol(&sem_analyzer, $1)) {
            Node *id_node = create_id_node($1);
            $$ = create_binop_node('=', id_node, (Node*)$3);
        } else {
            $$ = NULL;  // error occurred (e.g., duplicate declaration)
        }
    }
    ;

decl_item: ID '=' expr
    {
        // in declaration line: add symbol and create initialization
        if(sem_add_symbol(&sem_analyzer, $1)) {
            // check for division by zero in the expression
            if(!sem_check_division_by_zero((Node*)$3)) {
                fprintf(stderr, "Line %d: Division by zero in initialization\n", 
                        sem_analyzer.current_line);
                sem_analyzer.error_count++;
                $$ = NULL;
            } else {
                Node *id_node = create_id_node($1);
                $$ = create_binop_node('=', id_node, (Node*)$3);
            }
        } else {
            $$ = NULL;  // error occurred (e.g., duplicate declaration)
        }
    }
    ;

assign: ID '=' expr more_assign
    {
        // in assignment: check variable exists
        if(sem_check_declared(&sem_analyzer, $1)) {
            // check for division by zero
            if(!sem_check_division_by_zero((Node*)$3)) {
                fprintf(stderr, "Line %d: Division by zero in assignment\n", 
                        sem_analyzer.current_line);
                sem_analyzer.error_count++;
                $$ = NULL;
            } else {
                Node *id_node = create_id_node($1);
                Node *assign_expr = create_binop_node('=', id_node, (Node*)$3);
                
                // start building a list
                Node *assign_list = assign_expr;
                if($4) {
                    assign_list = append_to_list(assign_expr, (Node*)$4);
                }
                
                $$ = create_assign_node(assign_list);
            }
        } else {
            $$ = NULL;  // variable not declared
        }
    }
    ;

assign: ID '=' expr more_assign
    {
        // In assignment: check variable exists
        if(sem_check_declared(&sem_analyzer, $1)) {
            // Check for division by zero
            if(!sem_check_division_by_zero((Node*)$3)) {
                fprintf(stderr, "Line %d: Division by zero in assignment\n", 
                        sem_analyzer.current_line);
                sem_analyzer.error_count++;
                $$ = NULL;
            } else {
                Node *id_node = create_id_node($1);
                Node *assign_expr = create_binop_node('=', id_node, (Node*)$3);
                
                // Start building a list
                Node *assign_list = assign_expr;
                if($4) {
                    assign_list = append_to_list(assign_expr, (Node*)$4);
                }
                
                $$ = create_assign_node(assign_list);
            }
        } else {
            $$ = NULL;  // Variable not declared
        }
    }
    ;
    
more_assign: ',' ID '=' expr more_assign
    {
        // parse another assignment in the chain
        if(sem_check_declared(&sem_analyzer, $2)) {
            Node *id_node = create_id_node($2);
            Node *assign_expr = create_binop_node('=', id_node, (Node*)$4);
            
            // build list recursively
            Node *list = assign_expr;
            if($5) {
                list = append_to_list(assign_expr, (Node*)$5);
            }
            $$ = list;
        } else {
            $$ = NULL;  // variable not declared
        }
    }
    | /* epsilon */
    {
        $$ = NULL;
    }
    ;

print_stmt: KW_PRINT ':' print_parts
    {
        $$ = create_print_node((Node*)$3);
    }
    ;

print_parts: print_part more_print_parts
    {
    	//printf("DEBUG: Append print part, node type: %d\n", ((Node*)$1)->node_type);
        $$ = append_to_list((Node*)$1, (Node*)$2);
    }
    ;


more_print_parts: ',' print_part more_print_parts
    {
        //printf("DEBUG more_print_parts: matched with comma\n");
        $$ = append_to_list((Node*)$2, (Node*)$3);
    }
    | /* epsilon */
    {
        //printf("DEBUG more_print_parts: matched epsilon (empty)\n");
        $$ = NULL;
    }
    ;

// FIX ATTEMPT
print_part: STR
    {
        $$ = create_print_part_node(create_str_node($1));
    }
    | expr %prec PRINT_EXPR
    {
        $$ = create_print_part_node($1);
    }
    ;
////
    

expr: expr '+' term
    {
    	//printf("DEBUG: Creating addition expr\n"); // DEBUG
        $$ = create_binop_node('+', (Node*)$1, (Node*)$3);
    }
    | expr '-' term
    {
    	//printf("DEBUG: Creating subtraction expr\n"); // DEBUG
        $$ = create_binop_node('-', (Node*)$1, (Node*)$3);
    }
    | term
    {
        $$ = $1;
    }
    ;

term: term '*' factor
    {
        $$ = create_binop_node('*', (Node*)$1, (Node*)$3);
    }
    | term '/' factor
    {
        $$ = create_binop_node('/', (Node*)$1, (Node*)$3);
    }
    | factor
    {
        $$ = $1;
    }
    ;

factor: NUM
    {
        $$ = create_num_node($1);
    }
    | ID
    {
        if(sem_check_declared(&sem_analyzer, $1)) {
            $$ = create_id_node($1);
        } else {
            $$ = NULL;  // Error occurred
        }
    }
    | '(' expr ')'
    {
        $$ = $2;
    }
    | '-' factor
    {
        Node *neg_one = create_num_node(-1);
        $$ = create_binop_node('*', neg_one, (Node*)$2);
    }
    ;

%%

void print_ast(Node *node, int depth) {
    for(int i = 0; i < depth; i++) 
        printf("  ");
    if(!node) { 
        printf("NULL\n"); 
        return; 
    }
    
    printf("Node type: %d", node->node_type);
    switch(node->node_type) {
        case 0: printf(" (NUM) value: %d\n", node->int_val); break;
        case 1: printf(" (STR) value: %s\n", node->str_val); break;
        case 2: printf(" (ID) name: %s\n", node->str_val); break;
        case 3: printf(" (BINOP) op: %c\n", node->binop.op); 
                print_ast(node->binop.left, depth + 1);
                print_ast(node->binop.right, depth + 1);
                break;
        case 4: printf(" (DECL)\n"); 
                print_ast(node->list.items, depth + 1);
                print_ast(node->list.next, depth + 1);
                break;
        case 5: printf(" (ASSIGN)\n");
                print_ast(node->list.items, depth + 1);
                print_ast(node->list.next, depth + 1);
                break;
        /*case 6: printf(" (PRINT)\n");
                print_ast(node->print_stmt.parts, depth + 1);
                break;*/
        case 6: printf(" (PRINT)\n");
		{
		    Node *part = node->print_stmt.parts;
		    while(part) {
		        print_ast(part, depth + 1);
		        part = part->list.next;  // parts are linked via list.next
		    }
		}
		break;
        default: printf(" (UNKNOWN)\n");
    }
}

int main(int argc, char **argv) {
    if(argc < 2) {
        fprintf(stderr, "Usage: %s <input_file> [output_file]\n", argv[0]);
        return 1;
    }
    
    char *asm_filename = "MIPS64.s";
    char *machine_filename = "MACHINE_CODE.mc";
    
    if(argc >= 3) {
        asm_filename = argv[2];
        // create machine code filename from assembly filename
        char *dot = strrchr(asm_filename, '.');
        if(dot && strcmp(dot, ".s") == 0) {
            // replace .s with .mc
            strcpy(dot, ".mc");
            machine_filename = asm_filename;
            strcpy(dot, ".s"); // Restore .s
        } else {
            // append .mc
            machine_filename = malloc(strlen(asm_filename) + 4);
            sprintf(machine_filename, "%s.mc", asm_filename);
        }
    }
    
    // initialize semantic analyzer
    sem_init(&sem_analyzer);
    sem_set_line(&sem_analyzer, 1);
    
    yyin = fopen(argv[1], "r");
    if(!yyin) {
        fprintf(stderr, "Error: Cannot open file %s\n", argv[1]);
        sem_cleanup(&sem_analyzer);
        return 1;
    }
    
    int parse_result = yyparse();
    int error_count = sem_get_error_count(&sem_analyzer);
    
    if(parse_result == 0 && error_count == 0) {
        //printf("Compilation successful!\n");
        
        // debug: print AST structure
        //printf("\n=== AST Structure ===\n");
        //print_ast(ast_root, 0);
        //printf("====================\n\n");
        
        // open output file for assembly
        FILE *asm_file = fopen(asm_filename, "w");
        if(!asm_file) {
            fprintf(stderr, "Error: Cannot open assembly file %s\n", asm_filename);
            fclose(yyin);
            sem_cleanup(&sem_analyzer);
            free_node(ast_root);
            return 1;
        }
        
        // generate MIPS64 assembly
        GenerateAssemblyProgram(ast_root, asm_file);
        fclose(asm_file);
        
        //printf("MIPS64 assembly written to %s\n", asm_filename);
        
        // now convert assembly to machine code
        //printf("\n=== Converting assembly to machine code ===\n");
        if(MachineFromAssembly(asm_filename, machine_filename)) {
            //printf("Machine code written to %s\n", machine_filename);
            
            // display machine code
            //printf("\n=== Machine Code ===\n");
            //FILE *mc_file = fopen(machine_filename, "r");
            //if(mc_file) {
            //    char line[256];
            //    while(fgets(line, sizeof(line), mc_file)) {
            //        printf("%s", line);
            //    }
            //    fclose(mc_file);
           // }
        } else {
            fprintf(stderr, "Error converting assembly to machine code\n");
        }
        
        // now interpret the program and display output
        //printf("\n=== Program Output ===\n");
        char *output = interpret_program(ast_root);
        if(output && strlen(output) > 0) {
            printf("%s\n", output);
        } else {
            printf("(No output produced)\n");
        }
        free(output);
        
    } else {
        //printf("Compilation failed with %d error(s)\n", error_count);
    }
    
    fclose(yyin);
    yylex_destroy();
    sem_cleanup(&sem_analyzer);
    free_node(ast_root);
    
    return (parse_result != 0 || error_count > 0) ? 1 : 0;
}
void yyerror(const char *s) {
    //fprintf(stderr, "Syntax error at line %d: %s\n", sem_analyzer.current_line, s);
    sem_analyzer.error_count++;
}

/* AST Creation Functions */
Node *create_num_node(int val) {
    Node *node = malloc(sizeof(Node));
    node->node_type = 0;
    node->int_val = val;
    return node;
}

Node *create_str_node(char *str) {
    Node *node = malloc(sizeof(Node));
    node->node_type = 1;
    node->str_val = strdup(str);
    return node;
}

Node *create_id_node(char *name) {
    Node *node = malloc(sizeof(Node));
    node->node_type = 2;
    node->str_val = strdup(name);
    return node;
}

Node *create_binop_node(int op, Node *left, Node *right) {
    //printf("DEBUG create_binop_node: op='%c', left_type=%d, right_type=%d\n", 
    //       op, left ? left->node_type : -1, right ? right->node_type : -1);
    //Node *node = malloc(sizeof(Node));
    Node *node = calloc(1, sizeof(Node));
    node->node_type = 3;
    node->binop.op = op;
    node->binop.left = left;
    node->binop.right = right;
    return node;
}

Node *create_decl_node(Node *items) {
    //Node *node = malloc(sizeof(Node));
    Node *node = calloc(1, sizeof(Node));
    node->node_type = 4;
    node->list.items = items;
    node->list.next = NULL;
    return node;
}

Node *create_assign_node(Node *items) {
    //Node *node = malloc(sizeof(Node));
    Node *node = calloc(1, sizeof(Node));
    node->node_type = 5;
    node->list.items = items;
    node->list.next = NULL;
    return node;
}

Node *create_print_node(Node *parts) {
    //Node *node = malloc(sizeof(Node));
    Node *node = calloc(1, sizeof(Node));
    node->node_type = 6;
    node->print_stmt.parts = parts;
    return node;
}

/// FIX ATTEMPT
Node *create_print_part_node(Node *content) {
    Node *node = calloc(1, sizeof(Node));
    //Node *node = malloc(sizeof(Node));
    node->node_type = NODE_PRINT_PART;
    node->list.items = content;  // the actual content (STR, ID, BINOP, etc)
    node->list.next = NULL;      // next print part
    return node;
}
//////

Node *append_to_list(Node *first, Node *rest) {
    if(!first) 
        return rest;
    if(!rest)
        return first;
    
    // Both should be NODE_PRINT_PART nodes
    Node *current = first;
    while(current->list.next) {
        current = current->list.next;
    }
    current->list.next = rest;
    return first;
}
////

void free_node(Node *node) {
    if(!node)
        return;
    
    switch(node->node_type) {
        case 1: // STR
        case 2: // ID
            free(node->str_val);
            break;
        case 3: // BINOP
            free_node(node->binop.left);
            free_node(node->binop.right);
            break;
        case 4: // DECL
        case 5: // ASSIGN
        case 6: // PRINT
            free_node(node->list.items);
            free_node(node->list.next);
            break;
    }
    free(node);
}
