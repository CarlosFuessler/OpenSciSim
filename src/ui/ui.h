#ifndef UI_H
#define UI_H

#include "raylib.h"
#include "../modules/module.h"

#define MAX_MODULES 16

typedef enum {
    SCREEN_START,
    SCREEN_APP,
} ScreenState;

typedef struct AppUI {
    Module     *modules[MAX_MODULES];
    int         module_count;
    int         active_tab;
    ScreenState screen;
    float       start_time; // for animations
} AppUI;

void ui_init(AppUI *ui);
void ui_register_module(AppUI *ui, Module *mod);
void ui_update(AppUI *ui);
void ui_draw(AppUI *ui);

// Shared widgets
// Returns true if submitted (Enter pressed). insert_text is set if a template button inserts text.
bool ui_text_input(Rectangle bounds, char *buf, int buf_size, bool *active, const char *placeholder);

// Insert text into a buffer at the end (helper for template buttons)
void ui_buf_insert(char *buf, int buf_size, const char *text);

void ui_draw_button(Rectangle bounds, const char *text, bool hovered);

// Small template button, returns true if clicked
bool ui_template_btn(Rectangle bounds, const char *label, Color accent);

// Font-aware text drawing helpers
void ui_draw_text(const char *text, int x, int y, int fontSize, Color color);
int  ui_measure_text(const char *text, int fontSize);

// Prettify expression for display (x^2 -> x², sqrt -> √, pi -> π, etc.)
void ui_prettify_expr(const char *src, char *dst, int dst_size);

#endif
