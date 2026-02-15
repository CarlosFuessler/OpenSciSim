#include "eval.h"
#include <math.h>
#include <string.h>

double eval_ast(const ASTNode *node, double x) {
    return eval_ast_xy(node, x, 0.0);
}

double eval_ast_xy(const ASTNode *node, double x, double y) {
    if (!node) return NAN;

    switch (node->type) {
    case NODE_NUMBER:
        return node->number;

    case NODE_VAR:
        if (node->var == 'y') return y;
        return x;

    case NODE_UNARY_NEG:
        return -eval_ast_xy(node->unary.operand, x, y);

    case NODE_BINOP: {
        double l = eval_ast_xy(node->binop.left, x, y);
        double r = eval_ast_xy(node->binop.right, x, y);
        switch (node->binop.op) {
            case '+': return l + r;
            case '-': return l - r;
            case '*': return l * r;
            case '/': return r != 0.0 ? l / r : NAN;
            case '^': return pow(l, r);
            case '%': return r != 0.0 ? fmod(l, r) : NAN;
            default:  return NAN;
        }
    }

    case NODE_FUNC: {
        double a = eval_ast_xy(node->func.arg, x, y);
        const char *n = node->func.name;
        // Trigonometric
        if (strcmp(n, "sin")  == 0) return sin(a);
        if (strcmp(n, "cos")  == 0) return cos(a);
        if (strcmp(n, "tan")  == 0) return tan(a);
        if (strcmp(n, "asin") == 0) return asin(a);
        if (strcmp(n, "acos") == 0) return acos(a);
        if (strcmp(n, "atan") == 0) return atan(a);
        if (strcmp(n, "cot")  == 0) { double s = sin(a); return s != 0.0 ? cos(a) / s : NAN; }
        if (strcmp(n, "sec")  == 0) { double c = cos(a); return c != 0.0 ? 1.0 / c : NAN; }
        if (strcmp(n, "csc")  == 0) { double s = sin(a); return s != 0.0 ? 1.0 / s : NAN; }
        // Hyperbolic
        if (strcmp(n, "sinh") == 0) return sinh(a);
        if (strcmp(n, "cosh") == 0) return cosh(a);
        if (strcmp(n, "tanh") == 0) return tanh(a);
        if (strcmp(n, "asinh")== 0) return asinh(a);
        if (strcmp(n, "acosh")== 0) return acosh(a);
        if (strcmp(n, "atanh")== 0) return atanh(a);
        // Powers / roots
        if (strcmp(n, "sqrt") == 0) return sqrt(a);
        if (strcmp(n, "cbrt") == 0) return cbrt(a);
        // Logarithms
        if (strcmp(n, "log")  == 0) return log10(a);
        if (strcmp(n, "ln")   == 0) return log(a);
        if (strcmp(n, "log2") == 0) return log2(a);
        if (strcmp(n, "exp")  == 0) return exp(a);
        // Rounding / misc
        if (strcmp(n, "abs")  == 0) return fabs(a);
        if (strcmp(n, "floor")== 0) return floor(a);
        if (strcmp(n, "ceil") == 0) return ceil(a);
        if (strcmp(n, "round")== 0) return round(a);
        if (strcmp(n, "sign") == 0) return (a > 0.0) ? 1.0 : (a < 0.0) ? -1.0 : 0.0;
        // GeoGebra-style
        if (strcmp(n, "sgn")  == 0) return (a > 0.0) ? 1.0 : (a < 0.0) ? -1.0 : 0.0;
        return NAN;
    }
    }
    return NAN;
}
