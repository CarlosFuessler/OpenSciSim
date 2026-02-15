#ifndef EVAL_H
#define EVAL_H

#include "parser.h"

// Evaluate AST for a given value of x. Returns NAN on error.
double eval_ast(const ASTNode *node, double x);

// Evaluate AST for given values of x and y (for 3D surfaces). Returns NAN on error.
double eval_ast_xy(const ASTNode *node, double x, double y);

#endif
