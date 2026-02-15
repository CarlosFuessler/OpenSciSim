#ifndef UI_H
#define UI_H

#include "raylib.h"
#include "../modules/module.h"

#define MAX_MODULES 16
#define MAX_TOPICS   8
#define MAX_TOPIC_MODULES 8

typedef enum {
    SCREEN_START,   // Topic selection (home)
    SCREEN_TOPIC,   // Inside a topic, showing its sub-tabs
} ScreenState;

typedef struct {
    const char *name;
    const char *subtitle;
    Color       color;
    Module     *modules[MAX_TOPIC_MODULES];
    int         module_count;
    int         active_tab;
} Topic;

typedef struct AppUI {
    Topic       topics[MAX_TOPICS];
    int         topic_count;
    int         active_topic;
    ScreenState screen;
    float       start_time; // for animations
} AppUI;

void ui_init(AppUI *ui);

// Add a topic (returns topic index)
int  ui_add_topic(AppUI *ui, const char *name, const char *subtitle, Color color);

// Register a module under a topic
void ui_register_module(AppUI *ui, int topic_idx, Module *mod);

void ui_update(AppUI *ui);
void ui_draw(AppUI *ui);

// Responsive scaling + layout helpers
float ui_scale(void);
Vector2 ui_mouse(void);
Vector2 ui_to_screen(Vector2 ui_pos);
Vector2 ui_from_screen(Vector2 screen_pos);
void ui_scissor_begin(float x, float y, float w, float h);
Rectangle ui_pad(Rectangle bounds, float pad);
Rectangle ui_layout_row(Rectangle bounds, int count, int index, float gap, const float *weights);
Rectangle ui_layout_col(Rectangle bounds, int count, int index, float gap, const float *weights);

// Shared widgets
bool ui_text_input(Rectangle bounds, char *buf, int buf_size, bool *active, const char *placeholder);
void ui_buf_insert(char *buf, int buf_size, const char *text);
void ui_draw_button(Rectangle bounds, const char *text, bool hovered);
bool ui_template_btn(Rectangle bounds, const char *label, Color accent);

// Font-aware text drawing helpers
void ui_draw_text(const char *text, int x, int y, int fontSize, Color color);
int  ui_measure_text(const char *text, int fontSize);

// Prettify expression for display
void ui_prettify_expr(const char *src, char *dst, int dst_size);

#endif
