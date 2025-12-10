#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "interpreter.h"

#define NODE_PRINT_PART 7

// static void debug_print_ast(Node *node, int depth);

typedef struct Variable {
    char *name;
    int value;
    int initialized;
} Variable;

struct InterpreterState {
    Variable *vars;
    int var_count;
    int var_capacity;
    OutputCapture *output;
};

static Variable* find_variable(InterpreterState *state, const char *name) {
    for(int i = 0; i < state->var_count; i++) {
        if(strcmp(state->vars[i].name, name) == 0) {
            return &state->vars[i];
        }
    }
    return NULL;
}

static Variable* add_variable(InterpreterState *state, const char *name) {
    if(state->var_count >= state->var_capacity) {
        state->var_capacity = state->var_capacity ? state->var_capacity * 2 : 10;
        state->vars = realloc(state->vars, sizeof(Variable) * state->var_capacity);
    }
    
    Variable *var = &state->vars[state->var_count++];
    var->name = strdup(name);
    var->value = 0;
    var->initialized = 0;
    return var;
}

static InterpreterState* create_state() {
    InterpreterState *state = malloc(sizeof(InterpreterState));
    state->var_capacity = 10;
    state->var_count = 0;
    state->vars = malloc(sizeof(Variable) * state->var_capacity);
    state->output = malloc(sizeof(OutputCapture));
    capture_init(state->output);
    return state;
}

static void free_state(InterpreterState *state) {
    for(int i = 0; i < state->var_count; i++) {
        free(state->vars[i].name);
    }
    free(state->vars);
    if(state->output) {
        capture_free(state->output);
        free(state->output);
    }
    free(state);
}

static int evaluate_expression(Node *node, InterpreterState *state) {
    if(!node) return 0;
    
    switch(node->node_type) {
        case 0: // NODE_NUM
            return node->int_val;
            
        case 2: // NODE_ID
        {
            Variable *var = find_variable(state, node->str_val);
            if(!var) {
                var = add_variable(state, node->str_val);
            }
            if(!var->initialized) {
                // don't print warning - just use 0
            }
            return var->value;
        }
            
        case 3: // NODE_BINOP
        {
            int left = evaluate_expression(node->binop.left, state);
            int right = evaluate_expression(node->binop.right, state);
            
            switch(node->binop.op) {
                case '+': return left + right;
                case '-': return left - right;
                case '*': return left * right;
                case '/': return right != 0 ? left / right : 0;
                case '=': // should be handled in execute_statement
                    return left;
            }
            return 0;
        }
            
        default:
            return 0;
    }
}

static void execute_statement(Node *node, InterpreterState *state) {
    if(!node) return;
    
    switch(node->node_type) {
        case 4: // NODE_DECL
        {
            Node *current = node->list.items;
            while(current) {
                if(current->node_type == 3 && current->binop.op == '=') {
                    // declaration with initialization
                    Node *left = current->binop.left;
                    Node *right = current->binop.right;
                    
                    Variable *var = find_variable(state, left->str_val);
                    if(!var) {
                        var = add_variable(state, left->str_val);
                    }
                    
                    var->value = evaluate_expression(right, state);
                    var->initialized = 1;
                } else if(current->node_type == 2) {
                    // declaration without initialization
                    Variable *var = find_variable(state, current->str_val);
                    if(!var) {
                        var = add_variable(state, current->str_val);
                    }
                    var->initialized = 0;
                    var->value = 0;
                }
                current = current->list.next;
            }
            break;
        }
            
        case 5: // NODE_ASSIGN
        {
            Node *current = node->list.items;
            while(current) {
                if(current->node_type == 3 && current->binop.op == '=') {
                    Node *left = current->binop.left;
                    Node *right = current->binop.right;
                    
                    Variable *var = find_variable(state, left->str_val);
                    if(!var) {
                        var = add_variable(state, left->str_val);
                    }
                    
                    var->value = evaluate_expression(right, state);
                    var->initialized = 1;
                }
                current = current->list.next;
            }
            break;
        }
            
        // case 6: // NODE_PRINT
        // {
        //     Node *current = node->print_stmt.parts;
        //     while(current) {
        //         if(current->node_type == 1) {
        //             // sytring literal
        //             capture_printf(state->output, "%s", current->str_val);
        //         } else {
        //             // expression
        //             int value = evaluate_expression(current, state);
        //             capture_printf(state->output, "%d", value);
        //         }
        //         current = current->list.next;
        //     }
        //     break;
        // }

        // FIX ATTEMPT - to match changes in parser.y
        case 6: // NODE_PRINT
        {
            Node *current = node->print_stmt.parts;
            while(current) {
                if(current->node_type == NODE_PRINT_PART) {  // check for wrapper
                    Node *content = current->list.items;
                    if(content->node_type == 1) {  // STR
                        capture_printf(state->output, "%s", content->str_val);
                    } else {  // expression (ID, NUM, BINOP, etc)
                        int value = evaluate_expression(content, state);
                        capture_printf(state->output, "%d", value);
                    }
                    current = current->list.next;
                } else {
                    // old style (direct nodes) - for backward compatibility
                    if(current->node_type == 1) {  // STR
                        capture_printf(state->output, "%s", current->str_val);
                    } else {  // expression
                        int value = evaluate_expression(current, state);
                        capture_printf(state->output, "%d", value);
                    }
                    current = current->list.next;
                }
            }
            break;
        }
    }
}

char* interpret_program(Node *program) {
    InterpreterState *state = create_state();

    // execute all statements
    Node *current = program;
    while(current) {
        execute_statement(current, state);
        current = current->list.next;
    }
    
    char *result = strdup(capture_get(state->output));
    
    free_state(state);
    
    return result;
}


// // debugging....
// static void debug_print_ast(Node *node, int depth) {
//     for(int i = 0; i < depth; i++) 
//         printf("  ");
//     if(!node) { 
//             printf("NULL\n"); 
//             return; 
//     }
    
//     printf("Node type: %d", node->node_type);
//     switch (node->node_type) {
//         case 0: printf(" (NUM) value: %d\n", node->int_val); break;
//         case 1: printf(" (STR) value: %s\n", node->str_val); break;
//         case 2: printf(" (ID) name: %s\n", node->str_val); break;
//         case 3: printf(" (BINOP) op: %c\n", node->binop.op); 
//                 debug_print_ast(node->binop.left, depth + 1);
//                 debug_print_ast(node->binop.right, depth + 1);
//                 break;
//         case 4: printf(" (DECL)\n"); 
//                 debug_print_ast(node->list.items, depth + 1);
//                 debug_print_ast(node->list.next, depth + 1);
//                 break;
//         case 5: printf(" (ASSIGN)\n");
//                 debug_print_ast(node->list.items, depth + 1);
//                 debug_print_ast(node->list.next, depth + 1);
//                 break;
//         case 6: printf(" (PRINT)\n");
//                 debug_print_ast(node->print_stmt.parts, depth + 1);
//                 break;
//         default: printf(" (UNKNOWN)\n");
//     }
// }