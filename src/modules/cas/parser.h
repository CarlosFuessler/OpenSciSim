#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include "../../utils/arena.h"

typedef enum {
    NODE_NUMBER,
    NODE_VAR,       // x
    NODE_BINOP,     // +, -, *, /, ^
    NODE_UNARY_NEG, // unary minus
    NODE_FUNC,      // sin, cos, tan, sqrt, log, ln, abs, exp
} NodeType;

typedef struct ASTNode {
    NodeType type;
    union {
        double number;            // NODE_NUMBER
        char   var;               // NODE_VAR
        struct {                  // NODE_BINOP
            char op;
            struct ASTNode *left;
            struct ASTNode *right;
        } binop;
        struct {                  // NODE_UNARY_NEG
            struct ASTNode *operand;
        } unary;
        struct {                  // NODE_FUNC
            char name[16];
            struct ASTNode *arg;
        } func;
    };
} ASTNode;

typedef struct {
    const char *input;
    int         pos;
    Arena      *arena;
    char        error[128];
    bool        has_error;
} Parser;

void     parser_init(Parser *p, const char *input, Arena *arena);
ASTNode *parser_parse(Parser *p);

#endif
