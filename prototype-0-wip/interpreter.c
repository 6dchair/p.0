#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "interpreter.h"

#define NODE_PRINT_PART 7

// global flag to stop execution
static bool execution_stopped = false;

//static void debug_print_ast(Node *node, int depth);

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

// helper functions
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
    for(int i = 0; i < state->var_count; i++)
        free(state->vars[i].name);
    free(state->vars);
    if(state->output) {
        capture_free(state->output);
        free(state->output);
    }
    free(state);
}

// stop exec at first error
// updated: evaluate_expression to check if execution should stop
static int evaluate_expression(Node *node, InterpreterState *state, ErrorState *err) {
    if(!node || execution_stopped)
        return 0;
    
    switch(node->node_type) {
        case 0: // NODE_NUM
            return node->int_val;
            
        case 2: // NODE_ID
        {
            Variable *var = find_variable(state, node->str_val);
            if(!var)
                var = add_variable(state, node->str_val);
            if(!var->initialized) {
                report_uninitialized_variable(err, node->line_number, 0, node->str_val);
                execution_stopped = true;  // stop execution
                return 0;
            }
            return var->value;
        }
            
        case 3: // NODE_BINOP
        {
            int left = evaluate_expression(node->binop.left, state, err);
            if(execution_stopped)
                return 0;
            
            int right = evaluate_expression(node->binop.right, state, err);
            if(execution_stopped)
                return 0;
            
            switch(node->binop.op) {
                case '+': return left + right;
                case '-': return left - right;
                case '*': return left * right;
                case '/': 
                    if(right == 0) {
                        report_division_by_zero(err, node->line_number, 0);
                        execution_stopped = true;  // stop execution
                        return 0;
                    }
                    return left / right;
                case '=': // should be handled in execute_statement
                    return left;
            }
            return 0;
        }
            
        default:
            return 0;
    }
}


// updated execute_statement to check if execution should stop
static void execute_statement(Node *node, InterpreterState *state, ErrorState *err) {
    if(!node || execution_stopped)
        return;
    
    switch(node->node_type) {
        case 4: // NODE_DECL
        {
            Node *current = node->list.items;
            while(current && !execution_stopped) {
                if(current->node_type == 3 && current->binop.op == '=') {
                    Node *left = current->binop.left;
                    Node *right = current->binop.right;
                    
                    Variable *var = find_variable(state, left->str_val);
                    if(!var)
                        var = add_variable(state, left->str_val);
                    
                    var->value = evaluate_expression(right, state, err);
                    var->initialized = 1;
                } else if(current->node_type == 2) {
                    Variable *var = find_variable(state, current->str_val);
                    if(!var)
                        var = add_variable(state, current->str_val);
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
            while(current && !execution_stopped) {
                if(current->node_type == 3 && current->binop.op == '=') {
                    Node *left = current->binop.left;
                    Node *right = current->binop.right;
                    
                    Variable *var = find_variable(state, left->str_val);
                    if(!var) {
                        var = add_variable(state, left->str_val);
                    }
                    
                    var->value = evaluate_expression(right, state, err);
                    var->initialized = 1;
                }
                current = current->list.next;
            }
            break;
        }

        case 6: // NODE_PRINT
        {
            if(execution_stopped)
                return;
            
            Node *current = node->print_stmt.parts;
            
            // FIRST PASS: evaluate all expressions to check for errors B4 printing
            Node *temp = current;
            while(temp && !execution_stopped) {
                if(temp->node_type == NODE_PRINT_PART) {
                    Node *content = temp->list.items;
                    if(content->node_type != 1) { // not a string - evaluate to check for errors
                        evaluate_expression(content, state, err);
                    }
                } else if(temp->node_type != 1) { // legacy: not a string
                    evaluate_expression(temp, state, err);
                }
                temp = temp->list.next;
            }
            
            // if error occurred during evaluation, nothing is printed
            if(execution_stopped)
                return;
            
            // SECOND PASS: now actually print (no errors will occur)
            bool has_trailing_string = false;
            Node *last = NULL;
            temp = current;
            while(temp) {
                if(temp->node_type == NODE_PRINT_PART) {
                    Node *content = temp->list.items;
                    last = content;
                } else
                    last = temp;
                temp = temp->list.next;
            }
            
            if(last && last->node_type == 1) {
                char *str = last->str_val;
                int len = strlen(str);
                if(len > 0 && str[len-1] == '\n')
                    has_trailing_string = true;
            }
            
            temp = current;
            while(temp) {
                if(temp->node_type == NODE_PRINT_PART) {
                    Node *content = temp->list.items;
                    if(content->node_type == 1) {
                        capture_printf(state->output, "%s", content->str_val);
                    } else {
                        // reevaluate (safe since we alr checked for errors)
                        int value = evaluate_expression(content, state, err);
                        capture_printf(state->output, "%d", value);
                    }
                } else {
                    if(temp->node_type == 1) {
                        capture_printf(state->output, "%s", temp->str_val);
                    } else {
                        int value = evaluate_expression(temp, state, err);
                        capture_printf(state->output, "%d", value);
                    }
                }
                temp = temp->list.next;
            }
            
            if(!has_trailing_string) {
                capture_printf(state->output, "\n");
            }
            break;
        }
    }
}

char* interpret_program(Node *program, ErrorState *error_state) {
    InterpreterState *state = create_state();
    execution_stopped = false;
    
    Node *current = program;
    while(current && !execution_stopped) {
        execute_statement(current, state, error_state);
        current = current->list.next;
    }
    
    // if execution was stopped due to error, return empty string
    if(execution_stopped) {
        char *result = strdup("");
        free_state(state);
        return result;
    }
    
    char *result = strdup(capture_get(state->output));
    free_state(state);
    return result;
}

// // 4 debugging....
// static void debug_print_ast(Node *node, int depth) {
//     for(int i = 0; i < depth; i++)
//         printf("  ");
//     if(!node) { 
//         printf("NULL\n"); 
//         return;
//     }
    
//     printf("Node type: %d", node->node_type);
//     switch(node->node_type) {
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