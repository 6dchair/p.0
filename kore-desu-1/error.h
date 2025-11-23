// error.h
#ifndef ERROR_H
#define ERROR_H


// ErrorType: list of possible validation / semantic / syntax errors
typedef enum {
ERR_NONE,
ERR_UNDECLARED,
ERR_REDECLARED,
ERR_INVALID_IDENTIFIER,
ERR_MISSING_SEMICOLON,
ERR_INVALID_EXPRESSION,
ERR_SYNTAX,
ERR_KEYWORD_AS_IDENTIFIER
} ErrorType;


// Report an error to the user
// type: the error type
// line: the 1-based source line number where the error occurred
// extra: optional extra info (variable name, token text, etc.)
void ReportError(ErrorType type, int line, const char *extra);

#endif