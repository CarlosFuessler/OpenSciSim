#include "parser.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static ASTNode *parse_expr(Parser *p);
static ASTNode *parse_term(Parser *p);
static ASTNode *parse_power(Parser *p);
static ASTNode *parse_unary(Parser *p);
static ASTNode *parse_primary(Parser *p);

static void skip_ws(Parser *p) {
    while (p->input[p->pos] == ' ' || p->input[p->pos] == '\t')
        p->pos++;
}

static char peek(Parser *p) {
    skip_ws(p);
    return p->input[p->pos];
}

static char advance(Parser *p) {
    skip_ws(p);
    return p->input[p->pos++];
}

static void set_error(Parser *p, const char *msg) {
    if (!p->has_error) {
        snprintf(p->error, sizeof(p->error), "%s at position %d", msg, p->pos);
        p->has_error = true;
    }
}

static ASTNode *alloc_node(Parser *p) {
    ASTNode *n = arena_alloc(p->arena, sizeof(ASTNode));
    if (!n) set_error(p, "Out of memory");
    return n;
}

void parser_init(Parser *p, const char *input, Arena *arena) {
    p->input     = input;
    p->pos       = 0;
    p->arena     = arena;
    p->error[0]  = '\0';
    p->has_error = false;
}

ASTNode *parser_parse(Parser *p) {
    ASTNode *node = parse_expr(p);
    skip_ws(p);
    if (!p->has_error && p->input[p->pos] != '\0') {
        set_error(p, "Unexpected character");
    }
    return p->has_error ? NULL : node;
}

// expr = term (('+' | '-') term)*
static ASTNode *parse_expr(Parser *p) {
    ASTNode *left = parse_term(p);
    if (p->has_error) return NULL;

    while (peek(p) == '+' || peek(p) == '-') {
        char op = advance(p);
        ASTNode *right = parse_term(p);
        if (p->has_error) return NULL;

        ASTNode *node = alloc_node(p);
        if (!node) return NULL;
        node->type = NODE_BINOP;
        node->binop.op    = op;
        node->binop.left  = left;
        node->binop.right = right;
        left = node;
    }
    return left;
}

// term = power (('*' | '/' | '%') power)*
//        also handles implicit multiplication: 2x, 2sin(x), (a)(b)
static ASTNode *parse_term(Parser *p) {
    ASTNode *left = parse_power(p);
    if (p->has_error) return NULL;

    for (;;) {
        char c = peek(p);
        if (c == '*' || c == '/' || c == '%') {
            char op = advance(p);
            ASTNode *right = parse_power(p);
            if (p->has_error) return NULL;

            ASTNode *node = alloc_node(p);
            if (!node) return NULL;
            node->type = NODE_BINOP;
            node->binop.op    = op;
            node->binop.left  = left;
            node->binop.right = right;
            left = node;
        }
        // Implicit multiplication: number followed by var/func/paren
        // e.g. 2x, 3sin(x), 2(x+1), (a)(b)
        else if (c == '(' || isalpha(c) || c == '|') {
            // Only if left could be a coefficient (not an operator already)
            ASTNode *right = parse_power(p);
            if (p->has_error) return NULL;

            ASTNode *node = alloc_node(p);
            if (!node) return NULL;
            node->type = NODE_BINOP;
            node->binop.op    = '*';
            node->binop.left  = left;
            node->binop.right = right;
            left = node;
        }
        else {
            break;
        }
    }
    return left;
}

// power = unary ('^' power)?  (right-associative)
static ASTNode *parse_power(Parser *p) {
    ASTNode *base = parse_unary(p);
    if (p->has_error) return NULL;

    if (peek(p) == '^') {
        advance(p);
        ASTNode *exp = parse_power(p); // right-associative
        if (p->has_error) return NULL;

        ASTNode *node = alloc_node(p);
        if (!node) return NULL;
        node->type = NODE_BINOP;
        node->binop.op    = '^';
        node->binop.left  = base;
        node->binop.right = exp;
        return node;
    }
    return base;
}

// unary = '-' unary | primary
static ASTNode *parse_unary(Parser *p) {
    if (peek(p) == '-') {
        advance(p);
        ASTNode *operand = parse_unary(p);
        if (p->has_error) return NULL;

        ASTNode *node = alloc_node(p);
        if (!node) return NULL;
        node->type = NODE_UNARY_NEG;
        node->unary.operand = operand;
        return node;
    }
    return parse_primary(p);
}

// primary = NUMBER | VAR | FUNC '(' expr ')' | '(' expr ')' | '|' expr '|' | 'pi' | 'e'
static ASTNode *parse_primary(Parser *p) {
    skip_ws(p);
    char c = p->input[p->pos];

    // Number
    if (isdigit(c) || c == '.') {
        ASTNode *node = alloc_node(p);
        if (!node) return NULL;
        node->type = NODE_NUMBER;

        char numbuf[64];
        int ni = 0;
        while ((isdigit(p->input[p->pos]) || p->input[p->pos] == '.') && ni < 63) {
            numbuf[ni++] = p->input[p->pos++];
        }
        numbuf[ni] = '\0';
        node->number = strtod(numbuf, NULL);
        return node;
    }

    // Parenthesized expression
    if (c == '(') {
        p->pos++;
        ASTNode *inner = parse_expr(p);
        if (p->has_error) return NULL;
        if (peek(p) != ')') {
            set_error(p, "Expected ')'");
            return NULL;
        }
        p->pos++;
        return inner;
    }

    // Absolute value: |expr|
    if (c == '|') {
        p->pos++;
        ASTNode *inner = parse_expr(p);
        if (p->has_error) return NULL;
        skip_ws(p);
        if (p->input[p->pos] != '|') {
            set_error(p, "Expected closing '|'");
            return NULL;
        }
        p->pos++;
        ASTNode *node = alloc_node(p);
        if (!node) return NULL;
        node->type = NODE_FUNC;
        strcpy(node->func.name, "abs");
        node->func.arg = inner;
        return node;
    }

    // Identifier: variable, function, or constant
    if (isalpha(c) || c == '_') {
        char name[32];
        int ni = 0;
        while ((isalnum(p->input[p->pos]) || p->input[p->pos] == '_') && ni < 31) {
            name[ni++] = p->input[p->pos++];
        }
        name[ni] = '\0';

        // Constants
        if (strcmp(name, "pi") == 0) {
            ASTNode *node = alloc_node(p);
            if (!node) return NULL;
            node->type   = NODE_NUMBER;
            node->number = 3.14159265358979323846;
            return node;
        }
        if (strcmp(name, "e") == 0 && peek(p) != '(') {
            // 'e' as Euler's number only if not followed by '(' (which would be a function)
            ASTNode *node = alloc_node(p);
            if (!node) return NULL;
            node->type   = NODE_NUMBER;
            node->number = 2.71828182845904523536;
            return node;
        }

        // Single-letter variable (x or y)
        if (ni == 1 && (name[0] == 'x' || name[0] == 'y')) {
            ASTNode *node = alloc_node(p);
            if (!node) return NULL;
            node->type = NODE_VAR;
            node->var  = name[0];
            return node;
        }

        // Function call
        if (peek(p) == '(') {
            p->pos++; // skip '('
            ASTNode *arg = parse_expr(p);
            if (p->has_error) return NULL;
            if (peek(p) != ')') {
                set_error(p, "Expected ')' after function argument");
                return NULL;
            }
            p->pos++;

            ASTNode *node = alloc_node(p);
            if (!node) return NULL;
            node->type = NODE_FUNC;
            strncpy(node->func.name, name, 15);
            node->func.name[15] = '\0';
            node->func.arg = arg;
            return node;
        }

        set_error(p, "Unknown identifier");
        return NULL;
    }

    if (c == '\0') {
        set_error(p, "Unexpected end of input");
    } else {
        set_error(p, "Unexpected character");
    }
    return NULL;
}
