#include "cas.h"
#include "parser.h"
#include "eval.h"
#include "plotter.h"
#include "plotter3d.h"
#include "../../ui/ui.h"
#include "../../ui/theme.h"
#include "../../utils/arena.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define SIDEBAR_W     340
#define ROW_HEIGHT    38
#define ROW_GAP        4
#define TEMPLATE_H    28
#define TEMPLATE_W    48
#define TEMPLATE_GAP   4

static void cas_layout(Rectangle area, Rectangle *sidebar, Rectangle *plot_area, bool *side_by_side) {
    float aspect = (float)GetScreenWidth() / (float)GetScreenHeight();
    float gap = 12.0f;
    Rectangle content = ui_pad(area, 10.0f);
    bool side = aspect >= 1.35f;
    float weights_row[2] = {1.2f, 2.4f};
    float weights_col[2] = {1.6f, 2.4f};

    if (side) {
        *sidebar = ui_layout_row(content, 2, 0, gap, weights_row);
        *plot_area = ui_layout_row(content, 2, 1, gap, weights_row);
    } else {
        *sidebar = ui_layout_col(content, 2, 0, gap, weights_col);
        *plot_area = ui_layout_col(content, 2, 1, gap, weights_col);
    }
    if (side_by_side) *side_by_side = side;
}

typedef enum { MODE_2D, MODE_3D } CASMode;

static Arena       cas_arena;
static PlotState   plot;
static Plot3DState plot3d;
static char        error_msg[128];
static CASMode     cas_mode;

// Which slot's input field is currently active (-1 = none, MAX_FUNCTIONS = new row)
static int       active_field;
// New expression row buffer
static char      new_buf[EXPR_BUF_SIZE];
static bool      new_active;
// Scroll offset for function list
static float     scroll_y;

// 3D: vector input buffer
static char      vec_buf[VEC_BUF_SIZE];

static void cas_init(void) {
    cas_arena = arena_create(ARENA_DEFAULT_CAP);
    plotter_init(&plot);
    plotter3d_init(&plot3d);
    error_msg[0]  = '\0';
    active_field   = MAX_FUNCTIONS;
    new_buf[0]     = '\0';
    new_active     = true;
    scroll_y       = 0;
    cas_mode       = MODE_2D;
    vec_buf[0]     = '\0';
}

static void reparse_all(void) {
    arena_reset(&cas_arena);
    // 2D functions
    for (int i = 0; i < plot.func_count; i++) {
        Parser parser;
        parser_init(&parser, plot.funcs[i].expr_text, &cas_arena);
        plot.funcs[i].ast = parser_parse(&parser);
        plot.funcs[i].valid = !parser.has_error;
    }
    // 3D surface functions
    for (int i = 0; i < plot3d.surf_count; i++) {
        Parser parser;
        parser_init(&parser, plot3d.surfs[i].expr_text, &cas_arena);
        plot3d.surfs[i].ast = parser_parse(&parser);
        plot3d.surfs[i].valid = !parser.has_error;
    }
}

static void add_function(const char *expr) {
    if (plot.func_count >= MAX_FUNCTIONS) return;
    error_msg[0] = '\0';

    FuncSlot *slot = &plot.funcs[plot.func_count];
    strncpy(slot->expr_text, expr, EXPR_BUF_SIZE - 1);
    slot->expr_text[EXPR_BUF_SIZE - 1] = '\0';
    slot->visible = true;
    slot->color_idx = plot.func_count;
    snprintf(slot->name, FUNC_NAME_SIZE, "f%d", plot.func_count + 1);

    plot.func_count++;
    reparse_all();

    if (!slot->valid) {
        Parser parser;
        arena_reset(&cas_arena);
        parser_init(&parser, expr, &cas_arena);
        parser_parse(&parser);
        if (parser.has_error)
            snprintf(error_msg, sizeof(error_msg), "%s", parser.error);
        plot.func_count--;
        reparse_all();
    }
}

static void update_function(int index) {
    error_msg[0] = '\0';
    reparse_all();
    if (!plot.funcs[index].valid) {
        Parser parser;
        parser_init(&parser, plot.funcs[index].expr_text, &cas_arena);
        parser_parse(&parser);
        if (parser.has_error)
            snprintf(error_msg, sizeof(error_msg), "%s", parser.error);
    }
}

static void remove_function(int index) {
    if (index < 0 || index >= plot.func_count) return;
    for (int i = index; i < plot.func_count - 1; i++)
        plot.funcs[i] = plot.funcs[i + 1];
    plot.func_count--;
    for (int i = 0; i < plot.func_count; i++)
        snprintf(plot.funcs[i].name, FUNC_NAME_SIZE, "f%d", i + 1);
    reparse_all();
}

// ---- 3D surface functions ----

static void add_surface(const char *expr) {
    if (plot3d.surf_count >= MAX_FUNCTIONS) return;
    error_msg[0] = '\0';

    FuncSlot *slot = &plot3d.surfs[plot3d.surf_count];
    strncpy(slot->expr_text, expr, EXPR_BUF_SIZE - 1);
    slot->expr_text[EXPR_BUF_SIZE - 1] = '\0';
    slot->visible = true;
    slot->color_idx = plot3d.surf_count;
    snprintf(slot->name, FUNC_NAME_SIZE, "s%d", plot3d.surf_count + 1);

    plot3d.surf_count++;
    reparse_all();

    if (!slot->valid) {
        Parser parser;
        arena_reset(&cas_arena);
        parser_init(&parser, expr, &cas_arena);
        parser_parse(&parser);
        if (parser.has_error)
            snprintf(error_msg, sizeof(error_msg), "%s", parser.error);
        plot3d.surf_count--;
        reparse_all();
    }
}

static void update_surface(int index) {
    error_msg[0] = '\0';
    reparse_all();
    if (!plot3d.surfs[index].valid) {
        Parser parser;
        parser_init(&parser, plot3d.surfs[index].expr_text, &cas_arena);
        parser_parse(&parser);
        if (parser.has_error)
            snprintf(error_msg, sizeof(error_msg), "%s", parser.error);
    }
}

static void remove_surface(int index) {
    if (index < 0 || index >= plot3d.surf_count) return;
    for (int i = index; i < plot3d.surf_count - 1; i++)
        plot3d.surfs[i] = plot3d.surfs[i + 1];
    plot3d.surf_count--;
    for (int i = 0; i < plot3d.surf_count; i++)
        snprintf(plot3d.surfs[i].name, FUNC_NAME_SIZE, "s%d", i + 1);
    reparse_all();
}

// Parse vector string "x,y,z" and add to list
static void add_vector(const char *text) {
    if (plot3d.vec_count >= MAX_VECTORS) return;
    error_msg[0] = '\0';

    float vx = 0, vy = 0, vz = 0;
    if (sscanf(text, "%f,%f,%f", &vx, &vy, &vz) != 3) {
        snprintf(error_msg, sizeof(error_msg), "Vector format: x,y,z (e.g. 1,2,3)");
        return;
    }

    VecEntry *v = &plot3d.vecs[plot3d.vec_count];
    v->x = vx; v->y = vy; v->z = vz;
    v->color_idx = plot3d.vec_count + plot3d.surf_count; // offset to get different colors
    v->visible = true;
    strncpy(v->expr, text, VEC_BUF_SIZE - 1);
    v->expr[VEC_BUF_SIZE - 1] = '\0';
    snprintf(v->label, sizeof(v->label), "v%d", plot3d.vec_count + 1);
    plot3d.vec_count++;
}

static void remove_vector(int index) {
    if (index < 0 || index >= plot3d.vec_count) return;
    for (int i = index; i < plot3d.vec_count - 1; i++)
        plot3d.vecs[i] = plot3d.vecs[i + 1];
    plot3d.vec_count--;
    for (int i = 0; i < plot3d.vec_count; i++)
        snprintf(plot3d.vecs[i].label, sizeof(plot3d.vecs[i].label), "v%d", i + 1);
}

// Get pointer to the active buffer (existing slot or new row)
static char *get_active_buf(void) {
    if (cas_mode == MODE_3D) {
        if (active_field >= 0 && active_field < plot3d.surf_count)
            return plot3d.surfs[active_field].expr_text;
        return new_buf;
    }
    if (active_field >= 0 && active_field < plot.func_count)
        return plot.funcs[active_field].expr_text;
    return new_buf;
}

// Insert template text into the currently active field
static void insert_template(const char *text) {
    char *buf = get_active_buf();
    ui_buf_insert(buf, EXPR_BUF_SIZE, text);
    if (cas_mode == MODE_3D) {
        if (active_field >= 0 && active_field < plot3d.surf_count)
            update_surface(active_field);
    } else {
        if (active_field >= 0 && active_field < plot.func_count)
            update_function(active_field);
    }
}

static void cas_update(Rectangle area) {
    Rectangle sidebar = {0};
    Rectangle plot_area = {0};
    cas_layout(area, &sidebar, &plot_area, NULL);
    (void)sidebar;
    if (cas_mode == MODE_3D)
        plotter3d_update(&plot3d, plot_area);
    else
        plotter_update(&plot, plot_area);
}

// Draw a single function row with inline editing (shared for 2D/3D)
static float draw_func_row_ex(int index, float x, float y, float w,
                               FuncSlot *slots, int count,
                               void (*update_fn)(int), void (*remove_fn)(int),
                               const char *prefix) {
    (void)count;
    FuncSlot *slot = &slots[index];
    Color col = PLOT_COLORS[slot->color_idx % PLOT_COLOR_COUNT];
    Vector2 mouse = ui_mouse();
    bool is_editing = (active_field == index);

    // Row background
    Rectangle row = { x, y, w, ROW_HEIGHT };
    bool row_hovered = CheckCollisionPointRec(mouse, row);
    Color row_bg = is_editing ? COL_INPUT_BG : (row_hovered ? (Color){44, 46, 54, 255} : COL_PANEL);
    DrawRectangleRounded(row, 0.1f, 6, row_bg);

    // Color indicator bar
    DrawRectangleRounded(
        (Rectangle){x, y + 4, 4, ROW_HEIGHT - 8}, 1.0f, 4, col);

    // Visibility toggle
    Rectangle vis = { x + 10, y + (ROW_HEIGHT - 16) / 2, 16, 16 };
    bool vis_hov = CheckCollisionPointRec(mouse, vis);
    if (slot->visible) {
        DrawCircle((int)(vis.x + 8), (int)(vis.y + 8), 5, col);
        if (vis_hov) DrawCircleLines((int)(vis.x + 8), (int)(vis.y + 8), 7, WHITE);
    } else {
        DrawCircleLines((int)(vis.x + 8), (int)(vis.y + 8), 5, COL_TEXT_DIM);
    }
    if (vis_hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        slot->visible = !slot->visible;
    if (vis_hov && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
        slot->color_idx = (slot->color_idx + 1) % PLOT_COLOR_COUNT;

    // Label
    char label[16];
    snprintf(label, sizeof(label), "%s:", slot->name);
    ui_draw_text(label, (int)x + 30, (int)y + (ROW_HEIGHT - FONT_SIZE_SMALL) / 2,
                 FONT_SIZE_SMALL, col);

    int label_w = ui_measure_text(label, FONT_SIZE_SMALL);

    // Prefix (e.g. "z=" for 3D)
    int prefix_w = 0;
    float field_x = x + 34 + label_w;
    if (prefix && prefix[0]) {
        prefix_w = ui_measure_text(prefix, FONT_SIZE_SMALL);
        ui_draw_text(prefix, (int)field_x, (int)y + (ROW_HEIGHT - FONT_SIZE_SMALL) / 2,
                     FONT_SIZE_SMALL, COL_TEXT_DIM);
        field_x += prefix_w + 2;
    }

    float field_w = w - (field_x - x) - 28;
    Rectangle field_rect = { field_x, y + 2, field_w, ROW_HEIGHT - 4 };

    if (is_editing) {
        bool submitted = ui_text_input(field_rect, slot->expr_text, EXPR_BUF_SIZE,
                                       &new_active, "type expression...");
        if (submitted) {
            update_fn(index);
            active_field = MAX_FUNCTIONS;
            new_active = true;
        }
    } else {
        bool field_hov = CheckCollisionPointRec(mouse, field_rect);
        if (field_hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            active_field = index;
            new_active = true;
        }

        char pretty[512];
        ui_prettify_expr(slot->expr_text, pretty, sizeof(pretty));
        Color tc = slot->valid
            ? (slot->visible ? COL_TEXT : COL_TEXT_DIM)
            : COL_ERROR;
        ui_draw_text(pretty, (int)field_x + 4,
                     (int)y + (ROW_HEIGHT - FONT_SIZE_SMALL) / 2,
                     FONT_SIZE_SMALL, tc);
    }

    // Delete button
    Rectangle del = { x + w - 24, y + (ROW_HEIGHT - 18) / 2, 18, 18 };
    bool del_hov = CheckCollisionPointRec(mouse, del);
    if (del_hov)
        DrawRectangleRounded(del, 0.3f, 4,
                             (Color){COL_ERROR.r, COL_ERROR.g, COL_ERROR.b, 40});
    ui_draw_text("x", (int)del.x + 4, (int)del.y + 1, FONT_SIZE_TINY,
                 del_hov ? COL_ERROR : COL_TEXT_DIM);
    if (del_hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        remove_fn(index);
        if (active_field == index) active_field = MAX_FUNCTIONS;
        else if (active_field > index && active_field < MAX_FUNCTIONS) active_field--;
        return 0; // signal removed
    }

    return ROW_HEIGHT + ROW_GAP;
}

// 2D function row
static float draw_func_row(int index, float x, float y, float w) {
    return draw_func_row_ex(index, x, y, w,
                            plot.funcs, plot.func_count,
                            update_function, remove_function, NULL);
}

// 3D surface row
static float draw_surf_row(int index, float x, float y, float w) {
    return draw_func_row_ex(index, x, y, w,
                            plot3d.surfs, plot3d.surf_count,
                            update_surface, remove_surface, "z=");
}

// Draw a vector row
static float draw_vec_row(int index, float x, float y, float w) {
    VecEntry *v = &plot3d.vecs[index];
    Color col = PLOT_COLORS[v->color_idx % PLOT_COLOR_COUNT];
    Vector2 mouse = ui_mouse();

    Rectangle row = { x, y, w, ROW_HEIGHT };
    bool row_hovered = CheckCollisionPointRec(mouse, row);
    Color row_bg = row_hovered ? (Color){44, 46, 54, 255} : COL_PANEL;
    DrawRectangleRounded(row, 0.1f, 6, row_bg);

    DrawRectangleRounded(
        (Rectangle){x, y + 4, 4, ROW_HEIGHT - 8}, 1.0f, 4, col);

    // Visibility toggle
    Rectangle vis = { x + 10, y + (ROW_HEIGHT - 16) / 2, 16, 16 };
    bool vis_hov = CheckCollisionPointRec(mouse, vis);
    if (v->visible) {
        DrawCircle((int)(vis.x + 8), (int)(vis.y + 8), 5, col);
        if (vis_hov) DrawCircleLines((int)(vis.x + 8), (int)(vis.y + 8), 7, WHITE);
    } else {
        DrawCircleLines((int)(vis.x + 8), (int)(vis.y + 8), 5, COL_TEXT_DIM);
    }
    if (vis_hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        v->visible = !v->visible;
    if (vis_hov && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
        v->color_idx = (v->color_idx + 1) % PLOT_COLOR_COUNT;

    // Label + value
    char label[64];
    snprintf(label, sizeof(label), "%s: (%.1f, %.1f, %.1f)", v->label, v->x, v->y, v->z);
    ui_draw_text(label, (int)x + 30, (int)y + (ROW_HEIGHT - FONT_SIZE_SMALL) / 2,
                 FONT_SIZE_SMALL, col);

    // Delete
    Rectangle del = { x + w - 24, y + (ROW_HEIGHT - 18) / 2, 18, 18 };
    bool del_hov = CheckCollisionPointRec(mouse, del);
    if (del_hov)
        DrawRectangleRounded(del, 0.3f, 4,
                             (Color){COL_ERROR.r, COL_ERROR.g, COL_ERROR.b, 40});
    ui_draw_text("x", (int)del.x + 4, (int)del.y + 1, FONT_SIZE_TINY,
                 del_hov ? COL_ERROR : COL_TEXT_DIM);
    if (del_hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        remove_vector(index);
        return 0;
    }

    return ROW_HEIGHT + ROW_GAP;
}

static void draw_template_bar(float x, float y, float w) {
    typedef struct { const char *label; const char *insert; } Tmpl;
    Tmpl templates_2d[] = {
        {"x\xC2\xB2",   "x^2"},
        {"x\xC2\xB3",   "x^3"},
        {"x^n",          "x^"},
        {"\xE2\x88\x9A", "sqrt("},
        {"sin",          "sin("},
        {"cos",          "cos("},
        {"tan",          "tan("},
        {"ln",           "ln("},
        {"log",          "log("},
        {"exp",          "exp("},
        {"abs",          "|"},
        {"1/x",          "1/x"},
        {"\xCF\x80",    "pi"},
        {"e",            "e"},
    };
    Tmpl templates_3d[] = {
        {"x\xC2\xB2",   "x^2"},
        {"y\xC2\xB2",   "y^2"},
        {"x\xC2\xB7y",  "x*y"},
        {"\xE2\x88\x9A", "sqrt("},
        {"sin",          "sin("},
        {"cos",          "cos("},
        {"x^n",          "^"},
        {"ln",           "ln("},
        {"exp",          "exp("},
        {"abs",          "|"},
        {"\xCF\x80",    "pi"},
        {"e",            "e"},
    };

    Tmpl *templates;
    int count;
    if (cas_mode == MODE_3D) {
        templates = templates_3d;
        count = (int)(sizeof(templates_3d) / sizeof(templates_3d[0]));
    } else {
        templates = templates_2d;
        count = (int)(sizeof(templates_2d) / sizeof(templates_2d[0]));
    }

    float cx = x;
    float cy = y;
    for (int i = 0; i < count; i++) {
        if (cx + TEMPLATE_W > x + w) {
            cx = x;
            cy += TEMPLATE_H + TEMPLATE_GAP;
        }
        Rectangle btn = { cx, cy, TEMPLATE_W, TEMPLATE_H };
        if (ui_template_btn(btn, templates[i].label, COL_ACCENT)) {
            insert_template(templates[i].insert);
        }
        cx += TEMPLATE_W + TEMPLATE_GAP;
    }
}

// Draw the 2D/3D mode toggle
static float draw_mode_toggle(float x, float y, float w) {
    float btn_w = (w - 4) / 2;
    float btn_h = 28;

    Rectangle btn_2d = { x, y, btn_w, btn_h };
    Rectangle btn_3d = { x + btn_w + 4, y, btn_w, btn_h };

    Vector2 mouse = ui_mouse();

    // 2D button
    bool hov_2d = CheckCollisionPointRec(mouse, btn_2d);
    Color bg_2d = (cas_mode == MODE_2D) ? COL_ACCENT : (hov_2d ? (Color){50, 52, 62, 255} : COL_TAB);
    DrawRectangleRounded(btn_2d, 0.3f, 6, bg_2d);
    int tw2 = ui_measure_text("2D", FONT_SIZE_SMALL);
    ui_draw_text("2D", (int)(btn_2d.x + (btn_w - tw2) / 2),
                 (int)(btn_2d.y + (btn_h - FONT_SIZE_SMALL) / 2),
                 FONT_SIZE_SMALL, (cas_mode == MODE_2D) ? WHITE : COL_TEXT_DIM);
    if (hov_2d && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && cas_mode != MODE_2D) {
        cas_mode = MODE_2D;
        active_field = MAX_FUNCTIONS;
        new_buf[0] = '\0';
    }

    // 3D button
    bool hov_3d = CheckCollisionPointRec(mouse, btn_3d);
    Color bg_3d = (cas_mode == MODE_3D) ? COL_ACCENT : (hov_3d ? (Color){50, 52, 62, 255} : COL_TAB);
    DrawRectangleRounded(btn_3d, 0.3f, 6, bg_3d);
    int tw3 = ui_measure_text("3D", FONT_SIZE_SMALL);
    ui_draw_text("3D", (int)(btn_3d.x + (btn_w - tw3) / 2),
                 (int)(btn_3d.y + (btn_h - FONT_SIZE_SMALL) / 2),
                 FONT_SIZE_SMALL, (cas_mode == MODE_3D) ? WHITE : COL_TEXT_DIM);
    if (hov_3d && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && cas_mode != MODE_3D) {
        cas_mode = MODE_3D;
        active_field = MAX_FUNCTIONS;
        new_buf[0] = '\0';
    }

    return btn_h + 8;
}

static void cas_draw(Rectangle area) {
    // Layout
    Rectangle sidebar = {0};
    Rectangle plot_area = {0};
    bool side_by_side = true;
    cas_layout(area, &sidebar, &plot_area, &side_by_side);

    // Sidebar
    DrawRectangleRec(sidebar, COL_PANEL);
    if (side_by_side) {
        DrawLine((int)(sidebar.x + sidebar.width), (int)sidebar.y,
                 (int)(sidebar.x + sidebar.width), (int)(sidebar.y + sidebar.height), COL_GRID);
    } else {
        DrawLine((int)sidebar.x, (int)(sidebar.y + sidebar.height),
                 (int)(sidebar.x + sidebar.width), (int)(sidebar.y + sidebar.height), COL_GRID);
    }

    float sx = sidebar.x + 8;
    float sw = sidebar.width - 16;
    float sy = sidebar.y + 8;

    // Title
    const char *title = (cas_mode == MODE_3D) ? "3D Algebra" : "Algebra";
    ui_draw_text(title, (int)sx + 2, (int)sy, FONT_SIZE_LARGE, COL_ACCENT);
    sy += 32;

    // Mode toggle
    sy += draw_mode_toggle(sx, sy, sw);

    // Template buttons
    draw_template_bar(sx, sy, sw);
    int tmpl_count = (cas_mode == MODE_3D) ? 12 : 14;
    int per_row = (int)((sw + TEMPLATE_GAP) / (TEMPLATE_W + TEMPLATE_GAP));
    int tmpl_rows = (tmpl_count + per_row - 1) / per_row;
    sy += tmpl_rows * (TEMPLATE_H + TEMPLATE_GAP) + 8;

    // Separator
    DrawLine((int)sx, (int)sy, (int)(sx + sw), (int)sy, COL_GRID);
    sy += 8;

    float list_start = sy;
    float list_end = sidebar.y + sidebar.height - 80;

    ui_scissor_begin(sidebar.x, list_start, sidebar.width, list_end - list_start);

    float cy = list_start - scroll_y;

    if (cas_mode == MODE_2D) {
        // ---- 2D: function rows ----
        for (int i = 0; i < plot.func_count; i++) {
            float adv = draw_func_row(i, sx, cy, sw);
            if (adv == 0) { i--; continue; }
            cy += adv;
        }

        // New function input row
        {
            Color new_col = PLOT_COLORS[plot.func_count % PLOT_COLOR_COUNT];
            Rectangle new_row = { sx, cy, sw, ROW_HEIGHT };
            bool is_new_active = (active_field == MAX_FUNCTIONS);
            Color bg = is_new_active ? COL_INPUT_BG : (Color){36, 38, 46, 255};
            DrawRectangleRounded(new_row, 0.1f, 6, bg);

            DrawRectangleRounded(
                (Rectangle){sx, cy + 4, 4, ROW_HEIGHT - 8}, 1.0f, 4, new_col);

            ui_draw_text("+", (int)sx + 12, (int)cy + (ROW_HEIGHT - FONT_SIZE_SMALL) / 2,
                         FONT_SIZE_SMALL, new_col);

            float field_x = sx + 30;
            float field_w = sw - 34;
            Rectangle field_rect = { field_x, cy + 2, field_w, ROW_HEIGHT - 4 };

            if (is_new_active) {
                bool submitted = ui_text_input(field_rect, new_buf, EXPR_BUF_SIZE,
                                               &new_active, "new expression...");
                if (submitted && new_buf[0] != '\0') {
                    add_function(new_buf);
                    new_buf[0] = '\0';
                }
            } else {
                bool field_hov = CheckCollisionPointRec(ui_mouse(), field_rect);
                if (field_hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    active_field = MAX_FUNCTIONS;
                    new_active = true;
                }
                ui_draw_text("new expression...", (int)field_x + 4,
                             (int)cy + (ROW_HEIGHT - FONT_SIZE_SMALL) / 2,
                             FONT_SIZE_SMALL, COL_TEXT_DIM);
            }
            cy += ROW_HEIGHT + ROW_GAP;
        }
    } else {
        // ---- 3D: surface rows ----
        ui_draw_text("Surfaces  z = f(x,y)", (int)sx + 2, (int)cy, FONT_SIZE_SMALL, COL_TEXT_DIM);
        cy += 20;

        for (int i = 0; i < plot3d.surf_count; i++) {
            float adv = draw_surf_row(i, sx, cy, sw);
            if (adv == 0) { i--; continue; }
            cy += adv;
        }

        // New surface input
        {
            Color new_col = PLOT_COLORS[plot3d.surf_count % PLOT_COLOR_COUNT];
            Rectangle new_row = { sx, cy, sw, ROW_HEIGHT };
            bool is_new_active = (active_field == MAX_FUNCTIONS);
            Color bg = is_new_active ? COL_INPUT_BG : (Color){36, 38, 46, 255};
            DrawRectangleRounded(new_row, 0.1f, 6, bg);

            DrawRectangleRounded(
                (Rectangle){sx, cy + 4, 4, ROW_HEIGHT - 8}, 1.0f, 4, new_col);

            ui_draw_text("+", (int)sx + 12, (int)cy + (ROW_HEIGHT - FONT_SIZE_SMALL) / 2,
                         FONT_SIZE_SMALL, new_col);

            // z= prefix
            float zx = sx + 30;
            ui_draw_text("z=", (int)zx, (int)cy + (ROW_HEIGHT - FONT_SIZE_SMALL) / 2,
                         FONT_SIZE_SMALL, COL_TEXT_DIM);
            int zw = ui_measure_text("z=", FONT_SIZE_SMALL);
            float field_x = zx + zw + 2;
            float field_w = sw - (field_x - sx) - 4;
            Rectangle field_rect = { field_x, cy + 2, field_w, ROW_HEIGHT - 4 };

            if (is_new_active) {
                bool submitted = ui_text_input(field_rect, new_buf, EXPR_BUF_SIZE,
                                               &new_active, "e.g. x^2 + y^2");
                if (submitted && new_buf[0] != '\0') {
                    add_surface(new_buf);
                    new_buf[0] = '\0';
                }
            } else {
                bool field_hov = CheckCollisionPointRec(ui_mouse(), field_rect);
                if (field_hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    active_field = MAX_FUNCTIONS;
                    new_active = true;
                }
                ui_draw_text("e.g. x^2 + y^2", (int)field_x + 4,
                             (int)cy + (ROW_HEIGHT - FONT_SIZE_SMALL) / 2,
                             FONT_SIZE_SMALL, COL_TEXT_DIM);
            }
            cy += ROW_HEIGHT + ROW_GAP;
        }

        // Separator
        cy += 8;
        DrawLine((int)sx, (int)cy, (int)(sx + sw), (int)cy, COL_GRID);
        cy += 8;

        // ---- 3D: vector rows ----
        ui_draw_text("Vectors (x,y,z)", (int)sx + 2, (int)cy, FONT_SIZE_SMALL, COL_TEXT_DIM);
        cy += 20;

        for (int i = 0; i < plot3d.vec_count; i++) {
            float adv = draw_vec_row(i, sx, cy, sw);
            if (adv == 0) { i--; continue; }
            cy += adv;
        }

        // New vector input
        {
            Color new_col = PLOT_COLORS[(plot3d.vec_count + plot3d.surf_count) % PLOT_COLOR_COUNT];
            Rectangle new_row = { sx, cy, sw, ROW_HEIGHT };
            Color bg = (Color){36, 38, 46, 255};
            DrawRectangleRounded(new_row, 0.1f, 6, bg);
            DrawRectangleRounded(
                (Rectangle){sx, cy + 4, 4, ROW_HEIGHT - 8}, 1.0f, 4, new_col);

            ui_draw_text("+", (int)sx + 12, (int)cy + (ROW_HEIGHT - FONT_SIZE_SMALL) / 2,
                         FONT_SIZE_SMALL, new_col);

            float field_x = sx + 30;
            float field_w = sw - 34;
            Rectangle field_rect = { field_x, cy + 2, field_w, ROW_HEIGHT - 4 };

            // Use a separate active state for vector input
            static bool vec_active = false;
            bool submitted = ui_text_input(field_rect, vec_buf, VEC_BUF_SIZE,
                                           &vec_active, "x,y,z  e.g. 1,2,3");
            if (submitted && vec_buf[0] != '\0') {
                add_vector(vec_buf);
                vec_buf[0] = '\0';
            }
            cy += ROW_HEIGHT + ROW_GAP;
        }
    }

    EndScissorMode();

    // Scroll with mouse wheel when hovering sidebar
    Vector2 mouse = ui_mouse();
    if (CheckCollisionPointRec(mouse, sidebar)) {
        float wheel = GetMouseWheelMove();
        scroll_y -= wheel * 30.0f;
        float max_scroll = cy + scroll_y - list_end;
        if (max_scroll < 0) max_scroll = 0;
        if (scroll_y > max_scroll) scroll_y = max_scroll;
        if (scroll_y < 0) scroll_y = 0;
    }

    // Error message
    if (error_msg[0] != '\0') {
        ui_draw_text(error_msg, (int)sx + 2, (int)list_end + 4,
                     FONT_SIZE_SMALL - 2, COL_ERROR);
    }

    // Help footer
    float help_y = sidebar.y + sidebar.height - 54;
    DrawLine((int)sx, (int)help_y, (int)(sx + sw), (int)help_y, COL_GRID);
    help_y += 6;
    if (cas_mode == MODE_3D) {
        ui_draw_text("Drag=Orbit  Scroll=Zoom  Home=Reset", (int)sx, (int)help_y, FONT_SIZE_TINY, COL_TEXT_DIM);
        ui_draw_text("Surface: z = f(x,y)  e.g. sin(x)*cos(y)", (int)sx, (int)(help_y + 16), FONT_SIZE_TINY, COL_TEXT_DIM);
        ui_draw_text("Vector: x,y,z  Right-click=Color", (int)sx, (int)(help_y + 32), FONT_SIZE_TINY, COL_TEXT_DIM);
    } else {
        ui_draw_text("Scroll=Zoom  Drag=Pan  Home=Reset", (int)sx, (int)help_y, FONT_SIZE_TINY, COL_TEXT_DIM);
        ui_draw_text("Click row to edit  Right-click=Color", (int)sx, (int)(help_y + 16), FONT_SIZE_TINY, COL_TEXT_DIM);
        ui_draw_text("Implicit mul: 2x 3sin(x)  Abs: |x|", (int)sx, (int)(help_y + 32), FONT_SIZE_TINY, COL_TEXT_DIM);
    }

    // Plot area
    if (cas_mode == MODE_3D)
        plotter3d_draw(&plot3d, plot_area, &cas_arena);
    else
        plotter_draw(&plot, plot_area, &cas_arena);
}

static void cas_cleanup(void) {
    arena_destroy(&cas_arena);
}

static Module cas_mod = {
    .name    = "CAS Calculator",
    .init    = cas_init,
    .update  = cas_update,
    .draw    = cas_draw,
    .cleanup = cas_cleanup,
};

Module *cas_module(void) {
    return &cas_mod;
}
