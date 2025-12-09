%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "semantics.h"
#include "ast.h"
#include "assembly.h"
#include "machine_code.h"
#include "interpreter.h"

#define NODE_PRINT_PART 7 // FIX ATTEMPT

// recent: global variable to handle errors and lin enumbers
ErrorState error_state;

// AST root
Node *ast_root = NULL;

// global semantic analyzer
Semantics sem_analyzer;

extern int yylex();
extern int yyparse();
extern FILE *yyin;
void yyerror(const char *s);
int yylex_destroy(void);

// function prototypes; int line added to integrate error labeling and line numbers specification
Node *create_num_node(int val, int line);
Node *create_str_node(char *str, int line);
Node *create_id_node(char *name, int line);
Node *create_binop_node(int op, Node *left, Node *right, int line);
Node *create_decl_node(Node *items, int line);
Node *create_assign_node(Node *items, int line);
Node *create_print_node(Node *parts, int line);
Node *create_print_part_node(Node *content, int line);
Node *append_to_list(Node *list, Node *item);
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

decl: KW_INT decl_items
    {
        sem_set_decl_line(&sem_analyzer, true);  // we r currently in a declaration line
        $$ = create_decl_node((Node*)$2, sem_analyzer.current_line);
    }
    ;

decl_items: decl_item more_decl_items
    {
        $$ = append_to_list((Node*)$1, (Node*)$2);
    }
    ;

more_decl_items: ',' decl_item more_decl_items
    {
        $$ = append_to_list((Node*)$2, (Node*)$3);
    }
    | /* epsilon */
    {
        $$ = NULL;
    }
    ;

decl_item: ID
    {
        // in declaration line: just add symbol
        sem_add_symbol(&sem_analyzer, $1);
        $$ = create_id_node($1, sem_analyzer.current_line);  // division by 0 fix & add line number
    }
    | ID '=' expr
    {
        // in declaration line: add symbol and create initialization
        sem_add_symbol(&sem_analyzer, $1);
        Node *id_node = create_id_node($1, sem_analyzer.current_line);
        $$ = create_binop_node('=', id_node, (Node*)$3, sem_analyzer.current_line);
    }
    ;

assign: ID '=' expr more_assign
    {
        // in assignment: check variable exists
        if(sem_check_declared(&sem_analyzer, $1)) {
            Node *id_node = create_id_node($1, sem_analyzer.current_line);
            Node *assign_expr = create_binop_node('=', id_node, (Node*)$3, sem_analyzer.current_line);
            // sstart building a list
            Node *assign_list = assign_expr;
            if($4) {
                // $4 is a list of additional assignment expressions
                assign_list = append_to_list(assign_expr, (Node*)$4);
            }
    
            $$ = create_assign_node(assign_list, sem_analyzer.current_line);
        } else {
            $$ = NULL;
        }
    }
    ;

more_assign: ',' ID '=' expr more_assign
    {
        // parse another assignment in the chain
        if(sem_check_declared(&sem_analyzer, $2)) {
            Node *id_node = create_id_node($2, sem_analyzer.current_line);
            Node *assign_expr = create_binop_node('=', id_node, (Node*)$4, sem_analyzer.current_line);
            
            // build list recursively
            Node *list = assign_expr;
            if($5) {
                list = append_to_list(assign_expr, (Node*)$5);
            }
            $$ = list;
        } else {
            $$ = NULL;
        }
    }
    | /* epsilon */
    {
        $$ = NULL;
    }
    ;

print_stmt: KW_PRINT ':' print_parts
    {
        $$ = create_print_node((Node*)$3, sem_analyzer.current_line);
    }
    ;
    
/*print_stmt: KW_PRINT ':' print_parts
    {
        // count and debug parts
        int count = 0;
        Node *part = (Node*)$3;
        printf("DEBUG: Print parts:\n");
        while(part) {
            count++;
            printf("  Part %d: type %d", count, part->node_type);
            if(part->node_type == 1) printf(" (STR): %s\n", part->str_val);
            else if(part->node_type == 2) printf(" (ID): %s\n", part->str_val);
            else if(part->node_type == 3) printf(" (BINOP): op %c\n", part->binop.op);
            else printf("\n");
            part = part->list.next;
        }
        printf("DEBUG: Creating print node with %d parts\n", count);
        $$ = create_print_node((Node*)$3);
    }
    ;*/

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
        $$ = create_print_part_node(create_str_node($1, sem_analyzer.current_line),
                                    sem_analyzer.current_line); 
    }
    | expr %prec PRINT_EXPR
    {
        $$ = create_print_part_node($1, sem_analyzer.current_line);
    }
    ;
////
    

expr: expr '+' term
    {
    	//printf("DEBUG: Creating addition expr\n"); // DEBUG
         $$ = create_binop_node('+', (Node*)$1, (Node*)$3, sem_analyzer.current_line);
    }
    | expr '-' term
    {
    	//printf("DEBUG: Creating subtraction expr\n"); // DEBUG
        $$ = create_binop_node('-', (Node*)$1, (Node*)$3, sem_analyzer.current_line);
    }
    | term
    {
        $$ = $1;
    }
    ;

term: term '*' factor
    {
        $$ = create_binop_node('*', (Node*)$1, (Node*)$3, sem_analyzer.current_line);
    }
    | term '/' factor
    {
        $$ = create_binop_node('/', (Node*)$1, (Node*)$3, sem_analyzer.current_line);
    }
    | factor
    {
        $$ = $1;
    }
    ;

factor: NUM
    {
        $$ = create_num_node($1, sem_analyzer.current_line);
    }
    | ID
    {
        if(sem_check_declared(&sem_analyzer, $1)) {
            $$ = create_id_node($1, sem_analyzer.current_line);
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
        Node *neg_one = create_num_node(-1, sem_analyzer.current_line);
        $$ = create_binop_node('*', neg_one, (Node*)$2, sem_analyzer.current_line);
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

    // handle errors & line numbers
    // initialize error state
    error_state_init(&error_state);  // ADD THIS
    
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
        }
        
        // now interpret the program and display output
        //printf("\n=== Program Output ===\n");
        // interpret with error state
        char *output = interpret_program(ast_root, &error_state);
        
        // print runtime errors if any
        if(get_error_count(&error_state) > 0) {
            printf("\n=== Runtime Error ===\n");
            print_messages(&error_state);
            printf("====================\n");
            free(output);
        } else {
            // onnly print output if NO runtime errors
            if(output && strlen(output) > 0) {
                printf("%s", output);
            }
            free(output);
        }
        
    } else {
        // semantic/parsing error - don't execute at all
        printf("Compilation failed with %d error(s)\n", error_count);
    }
    
    fclose(yyin);
    yylex_destroy();
    sem_cleanup(&sem_analyzer);
    free_node(ast_root);
    error_state_free(&error_state);  // cleanup error state
    
    return (parse_result != 0 || error_count > 0) ? 1 : 0;
}

void yyerror(const char *s) {
    fprintf(stderr, "Syntax error at line %d: %s\n", sem_analyzer.current_line, s);
}

/* AST creation functions */
Node *create_num_node(int val, int line) {
    //Node *node = malloc(sizeof(Node));
    Node *node = calloc(1, sizeof(Node)); 
    node->node_type = 0;
    node->int_val = val;
    node->line_number = line; 
    return node;
}

Node *create_str_node(char *str, int line) {
    //Node *node = malloc(sizeof(Node));
    Node *node = calloc(1, sizeof(Node));
    node->node_type = 1;
    node->str_val = strdup(str);
    node->line_number = line; 
    return node;
}

Node *create_id_node(char *name, int line) {
    //Node *node = malloc(sizeof(Node));
    Node *node = calloc(1, sizeof(Node));
    node->node_type = 2;
    node->str_val = strdup(name);
    node->line_number = line; 
    return node;
}

Node *create_binop_node(int op, Node *left, Node *right, int line) {
    //printf("DEBUG create_binop_node: op='%c', left_type=%d, right_type=%d\n", 
    //       op, left ? left->node_type : -1, right ? right->node_type : -1);
    //Node *node = malloc(sizeof(Node));
    Node *node = calloc(1, sizeof(Node));
    node->node_type = 3;
    node->binop.op = op;
    node->binop.left = left;
    node->binop.right = right;
    node->line_number = line; 
    return node;
}

Node *create_decl_node(Node *items, int line) {
    //Node *node = malloc(sizeof(Node));
    Node *node = calloc(1, sizeof(Node));
    node->node_type = 4;
    node->list.items = items;
    node->list.next = NULL;
    node->line_number = line; 
    return node;
}

Node *create_assign_node(Node *items, int line) {
    //Node *node = malloc(sizeof(Node));
    Node *node = calloc(1, sizeof(Node));
    node->node_type = 5;
    node->list.items = items;
    node->list.next = NULL;
    node->line_number = line; 
    return node;
}

Node *create_print_node(Node *parts, int line) {
    //Node *node = malloc(sizeof(Node));
    Node *node = calloc(1, sizeof(Node));
    node->node_type = 6;
    node->print_stmt.parts = parts;
    node->line_number = line; 
    return node;
}

/// FIX ATTEMPT
Node *create_print_part_node(Node *content, int line) {
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
    
    // both should be NODE_PRINT_PART nodes
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
