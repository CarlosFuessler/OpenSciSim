#include "ui.h"
#include "theme.h"
#include <string.h>
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

void ui_init(AppUI *ui) {
    ui->module_count = 0;
    ui->active_tab   = 0;
    ui->screen       = SCREEN_START;
    ui->start_time   = 0.0f;
}

void ui_register_module(AppUI *ui, Module *mod) {
    if (ui->module_count >= MAX_MODULES) return;
    ui->modules[ui->module_count++] = mod;
    mod->init();
}

// Start screen
static void draw_start_screen(AppUI *ui) {
    float t = (float)GetTime() - ui->start_time;
    int w = GetScreenWidth();
    int h = GetScreenHeight();

    ClearBackground(COL_BG);

    // Animated floating sine wave background
    for (int px = 0; px < w; px += 3) {
        for (int layer = 0; layer < 3; layer++) {
            float freq = 0.008f + layer * 0.004f;
            float amp  = 40.0f + layer * 20.0f;
            float speed = 0.6f + layer * 0.3f;
            float phase = layer * 1.5f;
            float y = h * 0.55f + sinf(px * freq + t * speed + phase) * amp;
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

    // Title with fade-in
    float title_alpha = fminf(t * 1.5f, 1.0f);
    const char *title = "OpenSciSim";
    int title_w = ui_measure_text(title, FONT_SIZE_HERO);
    Color title_col = (Color){
        COL_ACCENT.r, COL_ACCENT.g, COL_ACCENT.b,
        (unsigned char)(title_alpha * 255)
    };
    ui_draw_text(title, (w - title_w) / 2, h / 2 - 100, FONT_SIZE_HERO, title_col);

    // Subtitle
    float sub_alpha = fminf(fmaxf((t - 0.5f) * 1.5f, 0.0f), 1.0f);
    const char *subtitle = "Interactive Science Simulator";
    int sub_w = ui_measure_text(subtitle, FONT_SIZE_LARGE);
    Color sub_col = (Color){
        COL_TEXT.r, COL_TEXT.g, COL_TEXT.b,
        (unsigned char)(sub_alpha * 255)
    };
    ui_draw_text(subtitle, (w - sub_w) / 2, h / 2 - 20, FONT_SIZE_LARGE, sub_col);

    // Version
    float ver_alpha = fminf(fmaxf((t - 1.0f) * 1.5f, 0.0f), 1.0f);
    const char *version = "v0.1.0  -  Physics  |  Mathematics  |  Chemistry";
    int ver_w = ui_measure_text(version, FONT_SIZE_SMALL);
    Color ver_col = (Color){
        COL_TEXT_DIM.r, COL_TEXT_DIM.g, COL_TEXT_DIM.b,
        (unsigned char)(ver_alpha * 255)
    };
    ui_draw_text(version, (w - ver_w) / 2, h / 2 + 20, FONT_SIZE_SMALL, ver_col);

    // "Press any key" blinking
    if (t > 1.5f) {
        float blink = (sinf(t * 3.0f) + 1.0f) * 0.5f;
        const char *prompt = "Press ENTER to start";
        int prompt_w = ui_measure_text(prompt, FONT_SIZE_DEFAULT);
        Color prompt_col = (Color){
            COL_ACCENT2.r, COL_ACCENT2.g, COL_ACCENT2.b,
            (unsigned char)(blink * 200 + 55)
        };
        ui_draw_text(prompt, (w - prompt_w) / 2, h / 2 + 80, FONT_SIZE_DEFAULT, prompt_col);
    }

    // Feature cards at the bottom
    if (t > 2.0f) {
        float card_alpha = fminf((t - 2.0f) * 2.0f, 1.0f);
        const char *features[] = {
            "CAS Calculator",
            "Function Plotter",
            "More Coming Soon"
        };
        const char *descriptions[] = {
            "Parse & evaluate expressions",
            "Interactive zoom & pan",
            "Physics, Chemistry, ..."
        };
        int card_count = 3;
        int card_w = 220;
        int card_h = 70;
        int total_w = card_count * card_w + (card_count - 1) * 16;
        int start_x = (w - total_w) / 2;
        int card_y = h / 2 + 140;

        for (int i = 0; i < card_count; i++) {
            int cx = start_x + i * (card_w + 16);
            Color card_bg = (Color){
                COL_PANEL.r, COL_PANEL.g, COL_PANEL.b,
                (unsigned char)(card_alpha * 200)
            };
            DrawRectangleRounded(
                (Rectangle){(float)cx, (float)card_y, (float)card_w, (float)card_h},
                0.1f, 8, card_bg
            );
            Color feat_col = (Color){
                PLOT_COLORS[i].r, PLOT_COLORS[i].g, PLOT_COLORS[i].b,
                (unsigned char)(card_alpha * 255)
            };
            ui_draw_text(features[i], cx + 12, card_y + 12, FONT_SIZE_DEFAULT, feat_col);
            Color desc_col = (Color){
                COL_TEXT_DIM.r, COL_TEXT_DIM.g, COL_TEXT_DIM.b,
                (unsigned char)(card_alpha * 255)
            };
            ui_draw_text(descriptions[i], cx + 12, card_y + 38, FONT_SIZE_SMALL - 2, desc_col);
        }
    }
}

void ui_update(AppUI *ui) {
    if (ui->screen == SCREEN_START) {
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            float t = (float)GetTime() - ui->start_time;
            if (t > 1.5f) ui->screen = SCREEN_APP;
        }
        return;
    }

    // Tab switching with mouse click
    Vector2 mouse = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mouse.y < TAB_HEIGHT) {
        float x = 0;
        for (int i = 0; i < ui->module_count; i++) {
            float tw = (float)ui_measure_text(ui->modules[i]->name, TAB_FONT_SIZE) + TAB_PADDING * 2;
            if (mouse.x >= x && mouse.x < x + tw) {
                ui->active_tab = i;
                break;
            }
            x += tw;
        }
    }

    // Tab switching with keyboard (Ctrl+Tab / Ctrl+Shift+Tab)
    if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
        if (IsKeyPressed(KEY_TAB)) {
            if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
                ui->active_tab = (ui->active_tab - 1 + ui->module_count) % ui->module_count;
            else
                ui->active_tab = (ui->active_tab + 1) % ui->module_count;
        }
    }

    // Escape key goes back to start screen
    if (IsKeyPressed(KEY_ESCAPE)) {
        ui->screen = SCREEN_START;
        ui->start_time = (float)GetTime();
    }

    // Update active module
    if (ui->module_count > 0) {
        Rectangle area = { 0, TAB_HEIGHT, (float)GetScreenWidth(), (float)GetScreenHeight() - TAB_HEIGHT };
        ui->modules[ui->active_tab]->update(area);
    }
}

void ui_draw(AppUI *ui) {
    if (ui->screen == SCREEN_START) {
        draw_start_screen(ui);
        return;
    }

    // Background
    ClearBackground(COL_BG);

    // Tab bar background
    DrawRectangle(0, 0, GetScreenWidth(), TAB_HEIGHT, COL_PANEL);

    // Tabs
    float x = 0;
    for (int i = 0; i < ui->module_count; i++) {
        float tw = (float)ui_measure_text(ui->modules[i]->name, TAB_FONT_SIZE) + TAB_PADDING * 2;
        bool active = (i == ui->active_tab);

        // Tab background with rounded top
        Color bg = active ? COL_TAB_ACT : COL_TAB;
        Color fg = active ? WHITE : COL_TEXT_DIM;

        DrawRectangle((int)x, 0, (int)tw, TAB_HEIGHT, bg);

        // Active indicator bar at bottom
        if (active) {
            DrawRectangle((int)x, TAB_HEIGHT - 3, (int)tw, 3, WHITE);
        }

        ui_draw_text(ui->modules[i]->name,
                     (int)(x + TAB_PADDING),
                     (TAB_HEIGHT - TAB_FONT_SIZE) / 2,
                     TAB_FONT_SIZE, fg);

        // Separator
        DrawLine((int)(x + tw), 4, (int)(x + tw), TAB_HEIGHT - 4, COL_BG);
        x += tw;
    }

    // Bottom line under tab bar
    DrawRectangle(0, TAB_HEIGHT - 1, GetScreenWidth(), 1, COL_GRID);

    // Home button (right side of tab bar)
    {
        const char *home_label = "< Home";
        int hw = ui_measure_text(home_label, FONT_SIZE_SMALL) + 16;
        Rectangle home_btn = {
            (float)(GetScreenWidth() - hw - 8), 6,
            (float)hw, (float)(TAB_HEIGHT - 12)
        };
        Vector2 mouse = GetMousePosition();
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
    }

    // Draw active module
    if (ui->module_count > 0) {
        Rectangle area = { 0, (float)TAB_HEIGHT, (float)GetScreenWidth(), (float)GetScreenHeight() - TAB_HEIGHT };
        ui->modules[ui->active_tab]->draw(area);
    }
}

bool ui_text_input(Rectangle bounds, char *buf, int buf_size, bool *active, const char *placeholder) {
    Vector2 mouse = GetMousePosition();
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
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);
    bool clicked = hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    Color bg = hovered ? (Color){accent.r, accent.g, accent.b, 50} : COL_INPUT_BG;
    DrawRectangleRounded(bounds, 0.25f, 6, bg);
    DrawRectangleRoundedLinesEx(bounds, 0.25f, 6, 1.0f,
                                hovered ? accent : COL_GRID);

    int tw = ui_measure_text(label, 13);
    ui_draw_text(label,
                 (int)(bounds.x + (bounds.width - tw) / 2),
                 (int)(bounds.y + (bounds.height - 13) / 2),
                 13, hovered ? accent : COL_TEXT_DIM);
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
