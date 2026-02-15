#include "calc.h"
#include "../../ui/ui.h"
#include "../../ui/theme.h"
#include "../cas/parser.h"
#include "../cas/eval.h"
#include "../../utils/arena.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

#define DISPLAY_BUF 512
#define HIST_MAX    64
#define HIST_LINE   128

typedef struct {
    char expr[HIST_LINE];
    char result[HIST_LINE];
} HistEntry;

static char      display[DISPLAY_BUF];
static HistEntry history[HIST_MAX];
static int       hist_count;
static Arena     calc_arena;
static float     hist_scroll;

// Button layout
typedef struct {
    const char *label;
    const char *insert; // text to insert (NULL = special action)
    int         action; // 0=insert, 1=eval, 2=clear, 3=backspace, 4=clear_entry
    Color       color;
} CalcBtn;

#define BTN_INS(l, t)     { l, t, 0, COL_TAB }
#define BTN_FUNC(l, t)    { l, t, 0, (Color){44, 80, 120, 255} }
#define BTN_OP(l, t)      { l, t, 0, (Color){56, 120, 220, 255} }
#define BTN_EVAL(l)       { l, NULL, 1, (Color){80, 180, 100, 255} }
#define BTN_CLR(l)        { l, NULL, 2, (Color){180, 60, 60, 255} }
#define BTN_BSP(l)        { l, NULL, 3, (Color){120, 80, 60, 255} }
#define BTN_CE(l)         { l, NULL, 4, (Color){160, 80, 60, 255} }

static const int BTN_COLS = 5;
static const int BTN_ROWS = 7;

static CalcBtn buttons[] = {
    // Row 1: functions
    BTN_FUNC("sin",  "sin("),
    BTN_FUNC("cos",  "cos("),
    BTN_FUNC("tan",  "tan("),
    BTN_FUNC("ln",   "ln("),
    BTN_FUNC("log",  "log("),
    // Row 2: more functions
    BTN_FUNC("sqrt", "sqrt("),
    BTN_FUNC("x^2",  "^2"),
    BTN_FUNC("x^y",  "^"),
    BTN_FUNC("(",    "("),
    BTN_FUNC(")",    ")"),
    // Row 3: special
    BTN_FUNC("|x|",  "|"),
    BTN_FUNC("pi",   "pi"),
    BTN_FUNC("e",    "e"),
    BTN_FUNC("exp",  "exp("),
    BTN_CLR("AC"),
    // Row 4: 7 8 9 / BS
    BTN_INS("7", "7"),
    BTN_INS("8", "8"),
    BTN_INS("9", "9"),
    BTN_OP("/",  "/"),
    BTN_BSP("DEL"),
    // Row 5: 4 5 6 * CE
    BTN_INS("4", "4"),
    BTN_INS("5", "5"),
    BTN_INS("6", "6"),
    BTN_OP("*",  "*"),
    BTN_CE("CE"),
    // Row 6: 1 2 3 -
    BTN_INS("1", "1"),
    BTN_INS("2", "2"),
    BTN_INS("3", "3"),
    BTN_OP("-",  "-"),
    BTN_EVAL("="),
    // Row 7: 0 . (-) +
    BTN_INS("0", "0"),
    BTN_INS("00", "00"),
    BTN_INS(".", "."),
    BTN_OP("+",  "+"),
    BTN_FUNC("ANS", NULL), // special: insert last answer
};

static char last_answer[HIST_LINE] = "0";

static void calc_init(void) {
    display[0]  = '\0';
    hist_count  = 0;
    hist_scroll = 0;
    calc_arena  = arena_create(ARENA_DEFAULT_CAP);
    strcpy(last_answer, "0");
}

static void evaluate_display(void) {
    if (display[0] == '\0') return;

    arena_reset(&calc_arena);
    Parser parser;
    parser_init(&parser, display, &calc_arena);
    ASTNode *ast = parser_parse(&parser);

    HistEntry *h = &history[hist_count % HIST_MAX];
    strncpy(h->expr, display, HIST_LINE - 1);
    h->expr[HIST_LINE - 1] = '\0';

    if (ast && !parser.has_error) {
        double val = eval_ast(ast, 0); // x=0 for plain calculator
        if (isnan(val)) {
            strcpy(h->result, "Error");
        } else if (val == (long long)val && fabs(val) < 1e15) {
            snprintf(h->result, HIST_LINE, "%lld", (long long)val);
        } else {
            snprintf(h->result, HIST_LINE, "%.10g", val);
        }
        strncpy(last_answer, h->result, HIST_LINE - 1);
        // Put result into display for chaining
        strncpy(display, h->result, DISPLAY_BUF - 1);
        display[DISPLAY_BUF - 1] = '\0';
    } else {
        snprintf(h->result, HIST_LINE, "Syntax error");
        display[0] = '\0';
    }

    hist_count++;
}

static void calc_update(Rectangle area) {
    (void)area;
    // Keyboard input for quick typing
    int ch;
    while ((ch = GetCharPressed()) != 0) {
        int len = (int)strlen(display);
        if (len < DISPLAY_BUF - 1) {
            display[len] = (char)ch;
            display[len + 1] = '\0';
        }
    }
    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
        int len = (int)strlen(display);
        if (len > 0) display[len - 1] = '\0';
    }
    if (IsKeyPressed(KEY_ENTER)) {
        evaluate_display();
    }
    if (IsKeyPressed(KEY_DELETE)) {
        display[0] = '\0';
    }
}

static void calc_draw(Rectangle area) {
    float cx = area.x + (area.width - 420) / 2;
    float cy = area.y + 10;
    float calc_w = 420;

    if (cx < area.x + 10) cx = area.x + 10;

    // Calculator title
    ui_draw_text("Calculator", (int)cx, (int)cy, FONT_SIZE_LARGE, COL_ACCENT);
    cy += 34;

    // Display
    float disp_h = 60;
    Rectangle disp_rect = { cx, cy, calc_w, disp_h };
    DrawRectangleRounded(disp_rect, 0.08f, 8, COL_INPUT_BG);
    DrawRectangleRoundedLinesEx(disp_rect, 0.08f, 8, 2.0f, COL_GRID);

    // Display text (right-aligned, large)
    int dlen = (int)strlen(display);
    if (dlen > 0) {
        // Prettify for display
        char pretty[DISPLAY_BUF];
        ui_prettify_expr(display, pretty, DISPLAY_BUF);
        int tw = ui_measure_text(pretty, FONT_SIZE_LARGE + 4);
        float tx = cx + calc_w - tw - 16;
        if (tx < cx + 8) tx = cx + 8;
        ui_draw_text(pretty, (int)tx, (int)(cy + (disp_h - FONT_SIZE_LARGE - 4) / 2),
                     FONT_SIZE_LARGE + 4, COL_TEXT);
    } else {
        ui_draw_text("0", (int)(cx + calc_w - ui_measure_text("0", FONT_SIZE_LARGE + 4) - 16),
                     (int)(cy + (disp_h - FONT_SIZE_LARGE - 4) / 2),
                     FONT_SIZE_LARGE + 4, COL_TEXT_DIM);
    }

    // Cursor blink
    if ((int)((float)GetTime() * 2) % 2 == 0) {
        char pretty[DISPLAY_BUF];
        ui_prettify_expr(display, pretty, DISPLAY_BUF);
        int tw = ui_measure_text(pretty, FONT_SIZE_LARGE + 4);
        float curs_x = cx + calc_w - 14;
        if (dlen > 0) {
            float tx = cx + calc_w - tw - 16;
            if (tx < cx + 8) tx = cx + 8;
            curs_x = tx + (float)tw + 2;
        }
        DrawRectangle((int)curs_x, (int)(cy + 14), 2, (int)(disp_h - 28), COL_ACCENT);
    }

    cy += disp_h + 8;

    // Button grid
    float btn_w = (calc_w - (BTN_COLS - 1) * 4) / BTN_COLS;
    float btn_h = 48;
    float btn_gap = 4;

    for (int r = 0; r < BTN_ROWS; r++) {
        for (int c = 0; c < BTN_COLS; c++) {
            int idx = r * BTN_COLS + c;
            if (idx >= (int)(sizeof(buttons) / sizeof(buttons[0]))) break;

            CalcBtn *btn = &buttons[idx];
            Rectangle brect = {
                cx + c * (btn_w + btn_gap),
                cy + r * (btn_h + btn_gap),
                btn_w, btn_h
            };

            Vector2 mouse = GetMousePosition();
            bool hovered = CheckCollisionPointRec(mouse, brect);
            bool clicked = hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

            // Draw button
            Color bg = hovered
                ? (Color){(unsigned char)(btn->color.r + 30),
                           (unsigned char)(btn->color.g + 30),
                           (unsigned char)(btn->color.b + 30), 255}
                : btn->color;
            DrawRectangleRounded(brect, 0.15f, 8, bg);
            if (hovered)
                DrawRectangleRoundedLinesEx(brect, 0.15f, 8, 1.5f, COL_TEXT);

            int tw = ui_measure_text(btn->label, FONT_SIZE_DEFAULT);
            ui_draw_text(btn->label,
                         (int)(brect.x + (brect.width - tw) / 2),
                         (int)(brect.y + (brect.height - FONT_SIZE_DEFAULT) / 2),
                         FONT_SIZE_DEFAULT, COL_TEXT);

            if (clicked) {
                switch (btn->action) {
                case 0: // insert
                    if (btn->insert) {
                        ui_buf_insert(display, DISPLAY_BUF, btn->insert);
                    } else {
                        // ANS button
                        ui_buf_insert(display, DISPLAY_BUF, last_answer);
                    }
                    break;
                case 1: // eval
                    evaluate_display();
                    break;
                case 2: // clear all
                    display[0] = '\0';
                    break;
                case 3: { // backspace
                    int len = (int)strlen(display);
                    if (len > 0) display[len - 1] = '\0';
                    break;
                }
                case 4: // clear entry
                    display[0] = '\0';
                    break;
                }
            }
        }
    }

    cy += BTN_ROWS * (btn_h + btn_gap) + 12;

    // History section
    float hist_top = cy;
    float hist_bottom = area.y + area.height - 8;
    if (hist_bottom > hist_top + 30) {
        DrawLine((int)cx, (int)hist_top, (int)(cx + calc_w), (int)hist_top, COL_GRID);
        ui_draw_text("History", (int)cx + 4, (int)hist_top + 4, FONT_SIZE_SMALL, COL_TEXT_DIM);
        float hy = hist_top + 24;

        BeginScissorMode((int)cx, (int)hy, (int)calc_w, (int)(hist_bottom - hy));

        // Show recent history (newest at top)
        int shown = 0;
        for (int i = hist_count - 1; i >= 0 && shown < HIST_MAX; i--, shown++) {
            HistEntry *h = &history[i % HIST_MAX];
            float row_y = hy + shown * 36 - hist_scroll;
            if (row_y > hist_bottom) break;
            if (row_y + 36 < hy) continue;

            // Expression (left, dim)
            char pretty_expr[512];
            ui_prettify_expr(h->expr, pretty_expr, sizeof(pretty_expr));
            ui_draw_text(pretty_expr, (int)cx + 4, (int)row_y, FONT_SIZE_SMALL, COL_TEXT_DIM);
            // Result (right, bright)
            char res_line[160];
            snprintf(res_line, sizeof(res_line), "= %s", h->result);
            int rw = ui_measure_text(res_line, FONT_SIZE_SMALL);
            bool is_error = (strcmp(h->result, "Syntax error") == 0 || strcmp(h->result, "Error") == 0);
            ui_draw_text(res_line, (int)(cx + calc_w - rw - 4), (int)row_y + 16,
                         FONT_SIZE_SMALL, is_error ? COL_ERROR : COL_ACCENT);
        }

        EndScissorMode();

        // Scroll history
        Vector2 mouse = GetMousePosition();
        Rectangle hist_area = { cx, hy, calc_w, hist_bottom - hy };
        if (CheckCollisionPointRec(mouse, hist_area)) {
            hist_scroll -= GetMouseWheelMove() * 30.0f;
            if (hist_scroll < 0) hist_scroll = 0;
            float max_s = (float)(hist_count * 36) - (hist_bottom - hy);
            if (max_s < 0) max_s = 0;
            if (hist_scroll > max_s) hist_scroll = max_s;
        }
    }
}

static void calc_cleanup(void) {
    arena_destroy(&calc_arena);
}

static Module calc_mod = {
    .name    = "Calculator",
    .init    = calc_init,
    .update  = calc_update,
    .draw    = calc_draw,
    .cleanup = calc_cleanup,
};

Module *calc_module(void) {
    return &calc_mod;
}
