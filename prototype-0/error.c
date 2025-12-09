#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void error_state_init(ErrorState *state) {
    state->messages = NULL;
    state->message_count = 0;
    state->message_capacity = 0;
    state->has_errors = false;
}

void error_state_free(ErrorState *state) {
    for(int i = 0; i < state->message_count; i++) {
        free(state->messages[i].message);
        free(state->messages[i].details);
    }
    free(state->messages);
    state->messages = NULL;
    state->message_count = 0;
    state->message_capacity = 0;
    state->has_errors = false;
}

static void ensure_capacity(ErrorState *state) {
    if(state->message_count >= state->message_capacity) {
        state->message_capacity = state->message_capacity == 0 ? 10 : state->message_capacity * 2;
        state->messages = realloc(state->messages, 
                                 sizeof(CompilerMessage) * state->message_capacity);
    }
}

void add_message(ErrorState *state, MessageType type, ErrorCode code, 
                 int line, int column, const char *message, const char *details) {
    ensure_capacity(state);
    
    CompilerMessage *msg = &state->messages[state->message_count++];
    msg->type = type;
    msg->code = code;
    msg->line = line;
    msg->column = column;
    msg->message = message ? strdup(message) : strdup(get_error_string(code));
    msg->details = details ? strdup(details) : NULL;
    
    if(type == MSG_ERROR) {
        state->has_errors = true;
    }
}

void add_error(ErrorState *state, ErrorCode code, int line, int column, const char *details) {
    add_message(state, MSG_ERROR, code, line, column, NULL, details);
}

void add_warning(ErrorState *state, ErrorCode code, int line, int column, const char *details) {
    add_message(state, MSG_WARNING, code, line, column, NULL, details);
}

void add_info(ErrorState *state, ErrorCode code, int line, int column, const char *details) {
    add_message(state, MSG_INFO, code, line, column, NULL, details);
}

// some error types will not be used, just extras for future uses or edits
const char* get_error_string(ErrorCode code) {
    switch(code) {
        case ERR_DIVISION_BY_ZERO:       
            return "Division by zero";
        case ERR_UNDECLARED_VARIABLE:    
            return "Undeclared variable";
        case ERR_REDECLARED_VARIABLE:    
            return "Redeclared variable";
        case ERR_UNINITIALIZED_VARIABLE: 
            return "Uninitialized variable";
        case ERR_SYNTAX_ERROR:           
            return "Syntax error";
        case ERR_TYPE_MISMATCH:          
            return "Type mismatch";
        case ERR_INVALID_OPERATOR:       
            return "Invalid operator";
        case ERR_MISSING_SEMICOLON:      
            return "Missing semicolon";
        case ERR_MISSING_PARENTHESIS:    
            return "Missing parenthesis";
        case ERR_INVALID_ESCAPE:         
            return "Invalid escape sequence";
        case ERR_FILE_NOT_FOUND:         
            return "File not found";
        case ERR_MEMORY_ALLOCATION:      
            return "Memory allocation error";
        case ERR_NONE:                   
            return "No error";
        default:                         
            return "Unknown error";
    }
}

void print_messages(ErrorState *state) {
    for(int i = 0; i < state->message_count; i++) {
        CompilerMessage *msg = &state->messages[i];
        
        const char *type_str;
        const char *color_code = "";
        const char *reset_code = "";
        
        #ifdef USE_COLOR
        switch(msg->type) {
            case MSG_ERROR:   color_code = "\033[1;31m"; type_str = "ERROR";   break;
            case MSG_WARNING: color_code = "\033[1;33m"; type_str = "WARNING"; break;
            case MSG_INFO:    color_code = "\033[1;36m"; type_str = "INFO";    break;
        }
        reset_code = "\033[0m";
        #else
        switch(msg->type) {
            case MSG_ERROR:   type_str = "ERROR";   break;
            case MSG_WARNING: type_str = "WARNING"; break;
            case MSG_INFO:    type_str = "INFO";    break;
        }
        #endif
        
        if(msg->line > 0) {
            if(msg->column > 0) {
                printf("%s%s:%s %s at line %d, column %d", 
                       color_code, type_str, reset_code, msg->message, msg->line, msg->column);
            } else {
                printf("%s%s:%s %s at line %d", 
                       color_code, type_str, reset_code, msg->message, msg->line);
            }
        } else {
            printf("%s%s:%s %s", color_code, type_str, reset_code, msg->message);
        }
        
        if(msg->details) {
            printf(" (%s)", msg->details);
        }
        printf("\n");
    }
}

void clear_messages(ErrorState *state) {
    error_state_free(state);
    error_state_init(state);
}

bool has_errors(ErrorState *state) {
    return state->has_errors;
}

int get_error_count(ErrorState *state) {
    int count = 0;
    for(int i = 0; i < state->message_count; i++) {
        if(state->messages[i].type == MSG_ERROR) {
            count++;
        }
    }
    return count;
}

int get_warning_count(ErrorState *state) {
    int count = 0;
    for(int i = 0; i < state->message_count; i++) {
        if(state->messages[i].type == MSG_WARNING) {
            count++;
        }
    }
    return count;
}

// common error reporting functions
void report_division_by_zero(ErrorState *state, int line, int column) {
    add_error(state, ERR_DIVISION_BY_ZERO, line, column, NULL);
}

void report_undeclared_variable(ErrorState *state, int line, int column, const char *var_name) {
    add_error(state, ERR_UNDECLARED_VARIABLE, line, column, var_name);
}

void report_redeclared_variable(ErrorState *state, int line, int column, const char *var_name) {
    add_error(state, ERR_REDECLARED_VARIABLE, line, column, var_name);
}

void report_uninitialized_variable(ErrorState *state, int line, int column, const char *var_name) {
    add_warning(state, ERR_UNINITIALIZED_VARIABLE, line, column, var_name);
}