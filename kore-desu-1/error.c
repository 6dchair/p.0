// error.c
#include <stdio.h>
#include "error.h"

// prints a human-readable error message
void ReportError(ErrorType type, int line, const char *extra) {
    switch(type) {
        case ERR_UNDECLARED:
            printf("\tError: Variable '%s' undeclared\n\n", extra);
            break;

        case ERR_REDECLARED:
            printf("\tError: Variable '%s' redeclared\n\n", extra);
            break;

        case ERR_INVALID_IDENTIFIER:
            printf("\tError: Invalid identifier '%s'\n\n", extra);
            break;

        case ERR_MISSING_SEMICOLON:
            printf("\tError: Missing semicolon\n\n");
            break;

        case ERR_INVALID_EXPRESSION:
            printf("\tError: Invalid expression\n\n");
            break;

        case ERR_KEYWORD_AS_IDENTIFIER:
            printf("\tError: '%s' is a keyword and can't be a variable name\n\n", extra);
            break;

        case ERR_SYNTAX:
            default:
            printf("\tError: Syntax error\n\n");
            break;
    }
}