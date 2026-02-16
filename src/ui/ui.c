#include "ui.h"
#include "theme.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

// Helper to draw with custom font
void ui_draw_text(const char *text, int x, int y, int fontSize, Color color) {
    DrawTextEx(g_font, text, (Vector2){(float)x, (float)y}, (float)fontSize, 1.0f, color);
}

int ui_measure_text(const char *text, int fontSize) {
    Vector2 sz = MeasureTextEx(g_font, text, (float)fontSize, 1.0f);
    return (int)sz.x;
}

static float g_ui_scale = 1.0f;
static Camera2D g_ui_camera = {0};

static void ui_update_scale(void) {
    float sx = (float)GetScreenWidth() / (float)WINDOW_W;
    float sy = (float)GetScreenHeight() / (float)WINDOW_H;
    g_ui_scale = fminf(sx, sy);
    if (g_ui_scale < UI_MIN_SCALE) g_ui_scale = UI_MIN_SCALE;
    if (g_ui_scale > UI_MAX_SCALE) g_ui_scale = UI_MAX_SCALE;

    float view_w = WINDOW_W * g_ui_scale;
    float view_h = WINDOW_H * g_ui_scale;
    g_ui_camera.offset = (Vector2){
        (GetScreenWidth() - view_w) * 0.5f,
        (GetScreenHeight() - view_h) * 0.5f
    };
    g_ui_camera.target = (Vector2){0, 0};
    g_ui_camera.rotation = 0.0f;
    g_ui_camera.zoom = g_ui_scale;
}

float ui_scale(void) {
    return g_ui_scale;
}

Vector2 ui_mouse(void) {
    return GetScreenToWorld2D(GetMousePosition(), g_ui_camera);
}

Vector2 ui_to_screen(Vector2 ui_pos) {
    return GetWorldToScreen2D(ui_pos, g_ui_camera);
}

Vector2 ui_from_screen(Vector2 screen_pos) {
    return GetScreenToWorld2D(screen_pos, g_ui_camera);
}

void ui_scissor_begin(float x, float y, float w, float h) {
    Vector2 tl = ui_to_screen((Vector2){x, y});
    Vector2 br = ui_to_screen((Vector2){x + w, y + h});
    float sx = fminf(tl.x, br.x);
    float sy = fminf(tl.y, br.y);
    float sw = fabsf(br.x - tl.x);
    float sh = fabsf(br.y - tl.y);
    BeginScissorMode((int)sx, (int)sy, (int)sw, (int)sh);
}

Rectangle ui_pad(Rectangle bounds, float pad) {
    return (Rectangle){
        bounds.x + pad,
        bounds.y + pad,
        bounds.width - pad * 2.0f,
        bounds.height - pad * 2.0f
    };
}

static Rectangle ui_layout_flex(Rectangle bounds, int count, int index, float gap, const float *weights, bool vertical) {
    if (count <= 0) return bounds;
    if (index < 0) index = 0;
    if (index >= count) index = count - 1;

    float total = 0.0f;
    for (int i = 0; i < count; i++) total += weights ? weights[i] : 1.0f;
    if (total <= 0.0f) total = 1.0f;

    float gap_total = gap * (count - 1);
    float avail = vertical ? bounds.height : bounds.width;
    avail -= gap_total;
    if (avail < 0) avail = 0;

    float cursor = vertical ? bounds.y : bounds.x;
    for (int i = 0; i < count; i++) {
        float w = (weights ? weights[i] : 1.0f) / total;
        float size = avail * w;
        if (i == index) {
            if (vertical) return (Rectangle){bounds.x, cursor, bounds.width, size};
            return (Rectangle){cursor, bounds.y, size, bounds.height};
        }
        cursor += size + gap;
    }
    return bounds;
}

Rectangle ui_layout_row(Rectangle bounds, int count, int index, float gap, const float *weights) {
    return ui_layout_flex(bounds, count, index, gap, weights, false);
}

Rectangle ui_layout_col(Rectangle bounds, int count, int index, float gap, const float *weights) {
    return ui_layout_flex(bounds, count, index, gap, weights, true);
}

void ui_init(AppUI *ui) {
    ui->topic_count  = 0;
    ui->active_topic = 0;
    ui->screen       = SCREEN_START;
    ui->start_time   = 0.0f;
    ui->show_help    = false;
}

int ui_add_topic(AppUI *ui, const char *name, const char *subtitle, Color color) {
    if (ui->topic_count >= MAX_TOPICS) return -1;
    int idx = ui->topic_count++;
    ui->topics[idx].name         = name;
    ui->topics[idx].subtitle     = subtitle;
    ui->topics[idx].color        = color;
    ui->topics[idx].module_count = 0;
    ui->topics[idx].active_tab   = 0;
    return idx;
}

void ui_register_module(AppUI *ui, int topic_idx, Module *mod) {
    if (topic_idx < 0 || topic_idx >= ui->topic_count) return;
    Topic *t = &ui->topics[topic_idx];
    if (t->module_count >= MAX_TOPIC_MODULES) return;
    t->modules[t->module_count++] = mod;
    mod->init();
}

// Start screen — topic selection
static void draw_start_screen(AppUI *ui) {
    float t = (float)GetTime() - ui->start_time;
    int w = WINDOW_W;
    int h = WINDOW_H;
    float aspect = (float)GetScreenWidth() / (float)GetScreenHeight();

    // Animated floating sine wave background
    for (int px = 0; px < w; px += 3) {
        for (int layer = 0; layer < 3; layer++) {
            float freq = 0.008f + layer * 0.004f;
            float amp  = 40.0f + layer * 20.0f;
            float speed = 0.6f + layer * 0.3f;
            float phase = layer * 1.5f;
            float y = h * 0.45f + sinf(px * freq + t * speed + phase) * amp;
            unsigned char alpha = (unsigned char)(20 + layer * 10);
            Color col = (Color){
                (unsigned char)(PLOT_COLORS[layer].r),
                (unsigned char)(PLOT_COLORS[layer].g),
                (unsigned char)(PLOT_COLORS[layer].b),
                alpha
            };
            DrawCircle(px, (int)y, 2.0f, col);
        }
    }

    // Title
    float title_alpha = fminf(t * 1.5f, 1.0f);
    const char *title = "OpenSciSim";
    int title_w = ui_measure_text(title, FONT_SIZE_HERO);
    Color title_col = (Color){
        COL_ACCENT.r, COL_ACCENT.g, COL_ACCENT.b,
        (unsigned char)(title_alpha * 255)
    };
    ui_draw_text(title, (w - title_w) / 2, h / 4 - 40, FONT_SIZE_HERO, title_col);

    // Subtitle
    float sub_alpha = fminf(fmaxf((t - 0.3f) * 2.0f, 0.0f), 1.0f);
    const char *subtitle = "Interactive Science Simulator";
    int sub_w = ui_measure_text(subtitle, FONT_SIZE_LARGE);
    Color sub_col = (Color){
        COL_TEXT.r, COL_TEXT.g, COL_TEXT.b,
        (unsigned char)(sub_alpha * 255)
    };
    ui_draw_text(subtitle, (w - sub_w) / 2, h / 4 + 40, FONT_SIZE_LARGE, sub_col);

    // "Choose a topic" prompt
    float prompt_alpha = fminf(fmaxf((t - 0.6f) * 2.0f, 0.0f), 1.0f);
    if (prompt_alpha > 0.01f) {
        const char *prompt = "Choose a topic to explore";
        int pw = ui_measure_text(prompt, FONT_SIZE_DEFAULT);
        Color pc = (Color){
            COL_TEXT_DIM.r, COL_TEXT_DIM.g, COL_TEXT_DIM.b,
            (unsigned char)(prompt_alpha * 255)
        };
        ui_draw_text(prompt, (w - pw) / 2, h / 2 - 20, FONT_SIZE_DEFAULT, pc);
    }

    // Topic cards
    float card_alpha = fminf(fmaxf((t - 0.8f) * 2.0f, 0.0f), 1.0f);
    if (card_alpha > 0.01f && ui->topic_count > 0) {
        int card_h = 150;
        int gap = 24;
        int cols = (aspect < 1.25f) ? 1 : ((aspect < 1.7f) ? 2 : 3);
        if (cols > ui->topic_count) cols = ui->topic_count;
        int rows = (ui->topic_count + cols - 1) / cols;

        float max_card_w = 320.0f;
        float min_card_w = 240.0f;
        float avail_w = w - gap * (cols + 1);
        float card_w = avail_w / cols;
        if (card_w > max_card_w) card_w = max_card_w;
        if (card_w < min_card_w) card_w = min_card_w;

        float grid_w = cols * card_w + (cols - 1) * gap;
        float grid_h = rows * card_h + (rows - 1) * gap;
        float grid_top = h / 2.0f + 10;
        float grid_bot_limit = h - 48;
        if (grid_top + grid_h > grid_bot_limit) {
            grid_top = grid_bot_limit - grid_h;
        }
        Rectangle grid = {
            (w - grid_w) / 2.0f,
            grid_top,
            grid_w, grid_h
        };

        Vector2 mouse = ui_mouse();

        for (int i = 0; i < ui->topic_count; i++) {
            Topic *topic = &ui->topics[i];
            int row = i / cols;
            int col = i % cols;
            int row_cols = cols;
            int remaining = ui->topic_count - row * cols;
            if (remaining < cols) row_cols = remaining;

            Rectangle row_bounds = ui_layout_col(grid, rows, row, gap, NULL);
            float row_w = row_cols * card_w + (row_cols - 1) * gap;
            Rectangle row_inner = row_bounds;
            row_inner.x += (row_bounds.width - row_w) / 2.0f;
            row_inner.width = row_w;
            Rectangle cell = ui_layout_row(row_inner, row_cols, col, gap, NULL);

            float cx = cell.x;
            float cy = cell.y;

            Rectangle card = { cx, cy, (float)card_w, (float)card_h };
            bool hovered = CheckCollisionPointRec(mouse, card);

            // Hover lift effect
            float lift = hovered ? -6.0f : 0.0f;
            Rectangle card_draw = { cx, cy + lift, (float)card_w, (float)card_h };

            // Card background
            Color bg = (Color){
                COL_PANEL.r, COL_PANEL.g, COL_PANEL.b,
                (unsigned char)(card_alpha * (hovered ? 240 : 200))
            };
            DrawRectangleRounded(card_draw, 0.08f, 8, bg);

            // Colored accent bar at top
            Color accent = (Color){
                topic->color.r, topic->color.g, topic->color.b,
                (unsigned char)(card_alpha * 255)
            };
            DrawRectangleRounded(
                (Rectangle){cx + 4, cy + lift + 4, (float)(card_w - 8), 4},
                0.5f, 4, accent
            );

            // Border on hover
            if (hovered) {
                DrawRectangleRoundedLinesEx(card_draw, 0.08f, 8, 2.0f, accent);
            }

            // Topic name
            Color name_col = (Color){
                topic->color.r, topic->color.g, topic->color.b,
                (unsigned char)(card_alpha * 255)
            };
            int nw = ui_measure_text(topic->name, FONT_SIZE_TITLE);
            ui_draw_text(topic->name,
                         (int)(cx + (card_w - nw) / 2),
                         (int)(cy + lift + 28),
                         FONT_SIZE_TITLE, name_col);

            // Subtitle
            Color sub = (Color){
                COL_TEXT_DIM.r, COL_TEXT_DIM.g, COL_TEXT_DIM.b,
                (unsigned char)(card_alpha * 255)
            };
            int sw2 = ui_measure_text(topic->subtitle, FONT_SIZE_SMALL);
            ui_draw_text(topic->subtitle,
                         (int)(cx + (card_w - sw2) / 2),
                         (int)(cy + lift + 80),
                         FONT_SIZE_SMALL, sub);

            // Module count
            char mod_info[32];
            snprintf(mod_info, sizeof(mod_info), "%d module%s",
                     topic->module_count, topic->module_count == 1 ? "" : "s");
            int mi_w = ui_measure_text(mod_info, FONT_SIZE_TINY);
            ui_draw_text(mod_info,
                         (int)(cx + (card_w - mi_w) / 2),
                         (int)(cy + lift + card_h - 28),
                         FONT_SIZE_TINY, (Color){COL_TEXT_DIM.r, COL_TEXT_DIM.g, COL_TEXT_DIM.b,
                                     (unsigned char)(card_alpha * 180)});

            // Click
            if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                ui->active_topic = i;
                ui->screen = SCREEN_TOPIC;
            }
        }
    }

    // Version at bottom
    {
        const char *ver = "v0.1.0";
        int vw = ui_measure_text(ver, FONT_SIZE_TINY);
        ui_draw_text(ver, (w - vw) / 2, h - 28, FONT_SIZE_TINY, COL_TEXT_DIM);
    }
}

void ui_update(AppUI *ui) {
    ui_update_scale();
    if (ui->screen == SCREEN_START) {
        // Topic selection handled in draw (click detection)
        return;
    }

    // SCREEN_TOPIC: inside a topic
    Topic *topic = &ui->topics[ui->active_topic];

    // Tab switching with mouse click
    Vector2 mouse = ui_mouse();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mouse.y < TAB_HEIGHT) {
        // Skip the topic label area
        float x = 0;
        // Topic label width
        float label_w = (float)ui_measure_text(topic->name, TAB_FONT_SIZE) + TAB_PADDING * 2;
        float sep_w = 20; // separator space
        x = label_w + sep_w;

        for (int i = 0; i < topic->module_count; i++) {
            float tw = (float)ui_measure_text(topic->modules[i]->name, TAB_FONT_SIZE) + TAB_PADDING * 2;
            if (mouse.x >= x && mouse.x < x + tw) {
                topic->active_tab = i;
                break;
            }
            x += tw;
        }
    }

    // Tab switching with keyboard (Ctrl+Tab / Ctrl+Shift+Tab)
    if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
        if (IsKeyPressed(KEY_TAB)) {
            if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
                topic->active_tab = (topic->active_tab - 1 + topic->module_count) % topic->module_count;
            else
                topic->active_tab = (topic->active_tab + 1) % topic->module_count;
        }
    }

    // Escape goes back to start screen
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (ui->show_help) {
            ui->show_help = false;
        } else {
            ui->screen = SCREEN_START;
            ui->start_time = (float)GetTime();
        }
    }

    // Toggle help overlay with H or ? (Shift+/)
    if (IsKeyPressed(KEY_H) || IsKeyPressed(KEY_SLASH)) {
        ui->show_help = !ui->show_help;
    }

    // Update active module
    if (topic->module_count > 0) {
        Rectangle area = { 0, TAB_HEIGHT, (float)WINDOW_W, (float)WINDOW_H - TAB_HEIGHT };
        topic->modules[topic->active_tab]->update(area);
    }
}

void ui_draw(AppUI *ui) {
    ClearBackground(COL_BG);
    BeginMode2D(g_ui_camera);
    if (ui->screen == SCREEN_START) {
        draw_start_screen(ui);
        EndMode2D();
        return;
    }

    // SCREEN_TOPIC
    Topic *topic = &ui->topics[ui->active_topic];

    // Tab bar
    DrawRectangle(0, 0, WINDOW_W, TAB_HEIGHT, COL_PANEL);

    float x = 0;

    // Topic label (colored, not clickable as tab)
    {
        float label_w = (float)ui_measure_text(topic->name, TAB_FONT_SIZE) + TAB_PADDING * 2;
        DrawRectangle(0, 0, (int)label_w, TAB_HEIGHT, (Color){
            (unsigned char)(topic->color.r * 0.3f),
            (unsigned char)(topic->color.g * 0.3f),
            (unsigned char)(topic->color.b * 0.3f), 255
        });
        DrawRectangle(0, TAB_HEIGHT - 3, (int)label_w, 3, topic->color);
        ui_draw_text(topic->name,
                     (int)TAB_PADDING,
                     (TAB_HEIGHT - TAB_FONT_SIZE) / 2,
                     TAB_FONT_SIZE, topic->color);

        // Separator line
        x = label_w;
        DrawLine((int)x, 6, (int)x, TAB_HEIGHT - 6, COL_GRID);
        x += 20;
    }

    // Module tabs
    for (int i = 0; i < topic->module_count; i++) {
        float tw = (float)ui_measure_text(topic->modules[i]->name, TAB_FONT_SIZE) + TAB_PADDING * 2;
        bool active = (i == topic->active_tab);

        Color bg = active ? COL_TAB_ACT : COL_TAB;
        Color fg = active ? WHITE : COL_TEXT_DIM;

        DrawRectangle((int)x, 0, (int)tw, TAB_HEIGHT, bg);
        if (active) {
            DrawRectangle((int)x, TAB_HEIGHT - 3, (int)tw, 3, WHITE);
        }

        ui_draw_text(topic->modules[i]->name,
                     (int)(x + TAB_PADDING),
                     (TAB_HEIGHT - TAB_FONT_SIZE) / 2,
                     TAB_FONT_SIZE, fg);

        DrawLine((int)(x + tw), 4, (int)(x + tw), TAB_HEIGHT - 4, COL_BG);
        x += tw;
    }

    DrawRectangle(0, TAB_HEIGHT - 1, WINDOW_W, 1, COL_GRID);

    // Home button (right side)
    {
        const char *home_label = "< Home";
        int hw = ui_measure_text(home_label, FONT_SIZE_SMALL) + 16;
        Rectangle home_btn = {
            (float)(WINDOW_W - hw - 8), 6,
            (float)hw, (float)(TAB_HEIGHT - 12)
        };
        Vector2 mouse = ui_mouse();
        bool hovered = CheckCollisionPointRec(mouse, home_btn);
        Color hbg = hovered ? (Color){60, 62, 72, 255} : COL_TAB;
        DrawRectangleRounded(home_btn, 0.3f, 6, hbg);
        ui_draw_text(home_label,
                     (int)(home_btn.x + 8),
                     (int)(home_btn.y + (home_btn.height - FONT_SIZE_SMALL) / 2),
                     FONT_SIZE_SMALL, hovered ? COL_TEXT : COL_TEXT_DIM);
        if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            ui->screen = SCREEN_START;
            ui->start_time = (float)GetTime();
        }

        // "[H] Help" hint left of Home button
        const char *help_hint = "[H] Help";
        int hhw = ui_measure_text(help_hint, FONT_SIZE_TINY);
        ui_draw_text(help_hint,
                     (int)(home_btn.x - hhw - 12),
                     (int)(home_btn.y + (home_btn.height - FONT_SIZE_TINY) / 2),
                     FONT_SIZE_TINY, COL_TEXT_DIM);
    }

    // Draw active module
    if (topic->module_count > 0) {
        Rectangle area = { 0, (float)TAB_HEIGHT, (float)WINDOW_W, (float)WINDOW_H - TAB_HEIGHT };
        topic->modules[topic->active_tab]->draw(area);
    }

    // Help overlay
    if (ui->show_help && topic->module_count > 0) {
        Module *mod = topic->modules[topic->active_tab];
        const char *help = mod->help_text ? mod->help_text : "No help available for this module.";

        DrawRectangle(0, 0, WINDOW_W, WINDOW_H, (Color){0, 0, 0, 160});

        float pad = 32;
        float box_w = 500;
        float box_x = (WINDOW_W - box_w) / 2;
        float box_y = WINDOW_H * 0.25f;

        /* measure text height */
        float line_h = FONT_SIZE_SMALL + 4;
        int lines = 1;
        for (const char *c = help; *c; c++) if (*c == '\n') lines++;
        float text_h = lines * line_h;
        float box_h = text_h + pad * 2 + 40;

        DrawRectangleRounded(
            (Rectangle){box_x, box_y, box_w, box_h},
            0.04f, 8, COL_PANEL);
        DrawRectangleRoundedLinesEx(
            (Rectangle){box_x, box_y, box_w, box_h},
            0.04f, 8, 2.0f, COL_ACCENT);

        /* title */
        char title[64];
        snprintf(title, sizeof(title), "%s - Help", mod->name);
        int tw = ui_measure_text(title, FONT_SIZE_DEFAULT);
        ui_draw_text(title,
                     (int)(box_x + (box_w - tw) / 2),
                     (int)(box_y + 14),
                     FONT_SIZE_DEFAULT, COL_ACCENT);

        /* help text, line by line */
        float ty = box_y + 44;
        const char *line_start = help;
        for (const char *c = help; ; c++) {
            if (*c == '\n' || *c == '\0') {
                char line_buf[256];
                int len = (int)(c - line_start);
                if (len >= (int)sizeof(line_buf)) len = (int)sizeof(line_buf) - 1;
                memcpy(line_buf, line_start, len);
                line_buf[len] = '\0';
                ui_draw_text(line_buf,
                             (int)(box_x + pad),
                             (int)ty,
                             FONT_SIZE_SMALL, COL_TEXT);
                ty += line_h;
                if (*c == '\0') break;
                line_start = c + 1;
            }
        }

        /* dismiss hint */
        const char *dismiss = "Press H or ESC to close";
        int dw = ui_measure_text(dismiss, FONT_SIZE_TINY);
        ui_draw_text(dismiss,
                     (int)(box_x + (box_w - dw) / 2),
                     (int)(box_y + box_h - 22),
                     FONT_SIZE_TINY, COL_TEXT_DIM);
    }

    EndMode2D();
}

bool ui_text_input(Rectangle bounds, char *buf, int buf_size, bool *active, const char *placeholder) {
    Vector2 mouse = ui_mouse();
    bool hovered = CheckCollisionPointRec(mouse, bounds);
    bool submitted = false;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        *active = hovered;
    }

    // Draw
    Color bg = *active ? COL_INPUT_BG : COL_PANEL;
    Color border = *active ? COL_ACCENT : COL_GRID;
    DrawRectangleRounded(bounds, 0.15f, 8, bg);
    DrawRectangleRoundedLinesEx(bounds, 0.15f, 8, 2.0f, border);

    int len = (int)strlen(buf);

    if (*active) {
        // Handle character input
        int ch;
        while ((ch = GetCharPressed()) != 0) {
            if (len < buf_size - 1) {
                buf[len++] = (char)ch;
                buf[len] = '\0';
            }
        }
        // Backspace
        if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
            if (len > 0) buf[--len] = '\0';
        }
        // Submit on Enter
        if (IsKeyPressed(KEY_ENTER)) {
            submitted = true;
        }

        // Cursor blink
        float time = (float)GetTime();
        if ((int)(time * 2) % 2 == 0) {
            int textW = ui_measure_text(buf, TAB_FONT_SIZE);
            DrawRectangle(
                (int)(bounds.x + 10 + textW),
                (int)(bounds.y + 8),
                2, (int)(bounds.height - 16),
                COL_ACCENT
            );
        }
    }

    if (len > 0) {
        ui_draw_text(buf,
                     (int)(bounds.x + 10),
                     (int)(bounds.y + (bounds.height - TAB_FONT_SIZE) / 2),
                     TAB_FONT_SIZE, COL_TEXT);
    } else if (!*active && placeholder) {
        ui_draw_text(placeholder,
                     (int)(bounds.x + 10),
                     (int)(bounds.y + (bounds.height - TAB_FONT_SIZE) / 2),
                     TAB_FONT_SIZE, COL_TEXT_DIM);
    }

    return submitted;
}

void ui_draw_button(Rectangle bounds, const char *text, bool hovered) {
    Color bg = hovered ? COL_TAB_ACT : COL_TAB;
    DrawRectangleRounded(bounds, 0.2f, 8, bg);
    if (hovered) DrawRectangleRoundedLinesEx(bounds, 0.2f, 8, 1.0f, WHITE);
    int tw = ui_measure_text(text, TAB_FONT_SIZE);
    ui_draw_text(text,
                 (int)(bounds.x + (bounds.width - tw) / 2),
                 (int)(bounds.y + (bounds.height - TAB_FONT_SIZE) / 2),
                 TAB_FONT_SIZE, COL_TEXT);
}

bool ui_template_btn(Rectangle bounds, const char *label, Color accent) {
    Vector2 mouse = ui_mouse();
    bool hovered = CheckCollisionPointRec(mouse, bounds);
    bool clicked = hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    Color bg = hovered ? (Color){accent.r, accent.g, accent.b, 50} : COL_INPUT_BG;
    DrawRectangleRounded(bounds, 0.25f, 6, bg);
    DrawRectangleRoundedLinesEx(bounds, 0.25f, 6, 1.0f,
                                hovered ? accent : COL_GRID);

    int tw = ui_measure_text(label, FONT_SIZE_TINY);
    ui_draw_text(label,
                 (int)(bounds.x + (bounds.width - tw) / 2),
                 (int)(bounds.y + (bounds.height - FONT_SIZE_TINY) / 2),
                 FONT_SIZE_TINY, hovered ? accent : COL_TEXT_DIM);
    return clicked;
}

void ui_buf_insert(char *buf, int buf_size, const char *text) {
    int len = (int)strlen(buf);
    int tlen = (int)strlen(text);
    if (len + tlen < buf_size - 1) {
        strcat(buf, text);
    }
}

// Prettify: x^2 -> x\xC2\xB2, x^3 -> x\xC2\xB3, sqrt(...) -> \xE2\x88\x9A(...),
//           pi -> \xCF\x80, * -> \xC2\xB7
void ui_prettify_expr(const char *src, char *dst, int dst_size) {
    int si = 0, di = 0;
    int slen = (int)strlen(src);

    while (si < slen && di < dst_size - 4) {
        // pi -> π (UTF-8: CF 80)
        if (si + 1 < slen && src[si] == 'p' && src[si+1] == 'i' &&
            (si + 2 >= slen || !isalnum((unsigned char)src[si+2])) &&
            (si == 0 || !isalnum((unsigned char)src[si-1]))) {
            dst[di++] = (char)0xCF; dst[di++] = (char)0x80;
            si += 2;
            continue;
        }
        // sqrt -> √ (UTF-8: E2 88 9A)
        if (si + 4 < slen && strncmp(src + si, "sqrt", 4) == 0 &&
            (si + 4 >= slen || src[si+4] == '(')) {
            dst[di++] = (char)0xE2; dst[di++] = (char)0x88; dst[di++] = (char)0x9A;
            si += 4;
            continue;
        }
        // ^2 -> ² (UTF-8: C2 B2)
        if (src[si] == '^' && si + 1 < slen && src[si+1] == '2' &&
            (si + 2 >= slen || !isdigit((unsigned char)src[si+2]))) {
            dst[di++] = (char)0xC2; dst[di++] = (char)0xB2;
            si += 2;
            continue;
        }
        // ^3 -> ³ (UTF-8: C2 B3)
        if (src[si] == '^' && si + 1 < slen && src[si+1] == '3' &&
            (si + 2 >= slen || !isdigit((unsigned char)src[si+2]))) {
            dst[di++] = (char)0xC2; dst[di++] = (char)0xB3;
            si += 2;
            continue;
        }
        // * -> · (UTF-8: C2 B7)
        if (src[si] == '*') {
            dst[di++] = (char)0xC2; dst[di++] = (char)0xB7;
            si++;
            continue;
        }
        dst[di++] = src[si++];
    }
    dst[di] = '\0';
}
