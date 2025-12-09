#ifndef ERROR_H
#define ERROR_H

#include <stdbool.h>

typedef enum {
    ERR_NONE = 0,
    ERR_DIVISION_BY_ZERO,
    ERR_UNDECLARED_VARIABLE,
    ERR_REDECLARED_VARIABLE,
    ERR_UNINITIALIZED_VARIABLE,
    ERR_SYNTAX_ERROR,
    ERR_TYPE_MISMATCH,
    ERR_INVALID_OPERATOR,
    ERR_MISSING_SEMICOLON,
    ERR_MISSING_PARENTHESIS,
    ERR_INVALID_ESCAPE,
    ERR_FILE_NOT_FOUND,
    ERR_MEMORY_ALLOCATION
} ErrorCode;

typedef enum {
    MSG_ERROR,
    MSG_WARNING,
    MSG_INFO
} MessageType;

typedef struct {
    MessageType type;
    ErrorCode code;
    int line;
    int column;
    char *message;
    char *details;
} CompilerMessage;

typedef struct {
    CompilerMessage *messages;
    int message_count;
    int message_capacity;
    bool has_errors;
} ErrorState;

// iniitialization and cleanup
void error_state_init(ErrorState *state);
void error_state_free(ErrorState *state);

// add messages
void add_message(ErrorState *state, MessageType type, ErrorCode code, 
                 int line, int column, const char *message, const char *details);
void add_error(ErrorState *state, ErrorCode code, int line, int column, const char *details);
void add_warning(ErrorState *state, ErrorCode code, int line, int column, const char *details);
void add_info(ErrorState *state, ErrorCode code, int line, int column, const char *details);

// utility functions
const char* get_error_string(ErrorCode code);
void print_messages(ErrorState *state);
void clear_messages(ErrorState *state);
bool has_errors(ErrorState *state);
int get_error_count(ErrorState *state);
int get_warning_count(ErrorState *state);

// common error functions
void report_division_by_zero(ErrorState *state, int line, int column);
void report_undeclared_variable(ErrorState *state, int line, int column, const char *var_name);
void report_redeclared_variable(ErrorState *state, int line, int column, const char *var_name);
void report_uninitialized_variable(ErrorState *state, int line, int column, const char *var_name);

#endif