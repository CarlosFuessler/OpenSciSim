#ifndef PLOTTER_H
#define PLOTTER_H

#include "raylib.h"
#include "parser.h"

#define MAX_FUNCTIONS 8
#define EXPR_BUF_SIZE 256
#define FUNC_NAME_SIZE 32

typedef struct {
    char     expr_text[EXPR_BUF_SIZE];
    char     name[FUNC_NAME_SIZE];  // custom name like "f1", "g", "velocity"
    ASTNode *ast;
    bool     valid;
    bool     visible;
    int      color_idx;
} FuncSlot;

typedef struct {
    // View window in math coordinates
    double center_x;
    double center_y;
    double scale; // pixels per unit

    // Functions
    FuncSlot funcs[MAX_FUNCTIONS];
    int      func_count;

    // Interaction state
    bool   dragging;
    Vector2 drag_start;
    double drag_cx, drag_cy;
} PlotState;

void plotter_init(PlotState *ps);
void plotter_update(PlotState *ps, Rectangle area);
void plotter_draw(PlotState *ps, Rectangle area, Arena *arena);

#endif
