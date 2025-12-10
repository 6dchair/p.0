// #endif
#ifndef AST_H
#define AST_H

#define NODE_PRINT_PART 7  

// AST Node structure
typedef struct Node {
    int node_type;
    union {
        int int_val;
        char *str_val;
        struct {
            struct Node *left;
            struct Node *right;
            int op;
        } binop;
        struct {
            struct Node *items;
            struct Node *next;
        } list;
        struct {
            struct Node *parts;
        } print_stmt;
    };
} Node;

void print_ast(Node *node, int depth);

#endif