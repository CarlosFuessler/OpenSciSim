#include "mathsim.h"
#include "../../ui/ui.h"
#include "../../ui/theme.h"
#include <math.h>
#include <stdio.h>

#define PI 3.14159265358979323846f

typedef enum {
    MATH_PARAM,
    MATH_POLAR
} MathMode;

typedef struct {
    const char *name;
    const char *eqx;
    const char *eqy;
    float tmin, tmax, step;
} ParamPreset;

typedef struct {
    const char *name;
    const char *eq;
    float tmin, tmax, step;
} PolarPreset;

static const ParamPreset param_presets[] = {
    {"Circle",        "cos(t)",                     "sin(t)",                     0, 2 * PI, 0.02f},
    {"Lissajous",     "sin(3t)",                    "sin(2t)",                    0, 2 * PI, 0.02f},
    {"Hypotrochoid",  "cos(t)+0.5cos(3t)",          "sin(t)-0.5sin(3t)",          0, 2 * PI, 0.02f},
    {"Spiral",        "0.1t cos(t)",                "0.1t sin(t)",                0, 8 * PI, 0.04f},
};
#define PARAM_COUNT (int)(sizeof(param_presets) / sizeof(param_presets[0]))

static const PolarPreset polar_presets[] = {
    {"Rose (k=4)",    "r = cos(4θ)",                0, 2 * PI, 0.01f},
    {"Spiral",        "r = 0.2θ",                   0, 8 * PI, 0.02f},
    {"Cardioid",      "r = 1 - cos(θ)",             0, 2 * PI, 0.01f},
    {"Lemniscate",    "r = sqrt(|cos(2θ)|)",        0, 2 * PI, 0.01f},
};
#define POLAR_COUNT (int)(sizeof(polar_presets) / sizeof(polar_presets[0]))

static int   math_mode = MATH_PARAM;
static int   param_idx = 0;
static int   polar_idx = 0;
static float zoom = 1.0f;

static void math_layout(Rectangle area, Rectangle *panel, Rectangle *plot, bool *side_by_side) {
    float aspect = (float)GetScreenWidth() / (float)GetScreenHeight();
    float gap = 12.0f;
    Rectangle content = ui_pad(area, 10.0f);
    bool side = aspect >= 1.35f;
    float weights_row[2] = {1.1f, 2.5f};
    float weights_col[2] = {1.4f, 2.6f};

    if (side) {
        *panel = ui_layout_row(content, 2, 0, gap, weights_row);
        *plot  = ui_layout_row(content, 2, 1, gap, weights_row);
    } else {
        *panel = ui_layout_col(content, 2, 0, gap, weights_col);
        *plot  = ui_layout_col(content, 2, 1, gap, weights_col);
    }
    if (side_by_side) *side_by_side = side;
}

static Vector2 math_to_screen(Rectangle area, float scale, float x, float y) {
    return (Vector2){
        area.x + area.width * 0.5f + x * scale,
        area.y + area.height * 0.5f - y * scale
    };
}

static float nice_step(float raw) {
    if (raw <= 0.0f) return 1.0f;
    float mag = powf(10.0f, floorf(log10f(raw)));
    float norm = raw / mag;
    if (norm < 2.0f) return 2.0f * mag;
    if (norm < 5.0f) return 5.0f * mag;
    return 10.0f * mag;
}

static void draw_grid(Rectangle area, float scale) {
    float raw_step = 60.0f / scale;
    float step = nice_step(raw_step);

    float x_min = -(area.width / 2.0f) / scale;
    float x_max =  (area.width / 2.0f) / scale;
    float y_min = -(area.height / 2.0f) / scale;
    float y_max =  (area.height / 2.0f) / scale;

    float sub_step = step / 5.0f;
    Color sub_col = (Color){42, 44, 50, 255};

    float sx_start = floorf(x_min / sub_step) * sub_step;
    for (float gx = sx_start; gx <= x_max; gx += sub_step) {
        Vector2 top = math_to_screen(area, scale, gx, y_max);
        Vector2 bot = math_to_screen(area, scale, gx, y_min);
        DrawLineV(top, bot, sub_col);
    }
    float sy_start = floorf(y_min / sub_step) * sub_step;
    for (float gy = sy_start; gy <= y_max; gy += sub_step) {
        Vector2 left  = math_to_screen(area, scale, x_min, gy);
        Vector2 right = math_to_screen(area, scale, x_max, gy);
        DrawLineV(left, right, sub_col);
    }

    float x_start = floorf(x_min / step) * step;
    for (float gx = x_start; gx <= x_max; gx += step) {
        Vector2 top = math_to_screen(area, scale, gx, y_max);
        Vector2 bot = math_to_screen(area, scale, gx, y_min);
        DrawLineV(top, bot, COL_GRID);
    }
    float y_start = floorf(y_min / step) * step;
    for (float gy = y_start; gy <= y_max; gy += step) {
        Vector2 left  = math_to_screen(area, scale, x_min, gy);
        Vector2 right = math_to_screen(area, scale, x_max, gy);
        DrawLineV(left, right, COL_GRID);
    }

    // Axes
    Vector2 ax1 = math_to_screen(area, scale, x_min, 0);
    Vector2 ax2 = math_to_screen(area, scale, x_max, 0);
    Vector2 ay1 = math_to_screen(area, scale, 0, y_min);
    Vector2 ay2 = math_to_screen(area, scale, 0, y_max);
    DrawLineV(ax1, ax2, COL_AXIS);
    DrawLineV(ay1, ay2, COL_AXIS);
}

static void eval_param(int idx, float t, float *x, float *y) {
    switch (idx) {
        case 0: *x = cosf(t); *y = sinf(t); break;
        case 1: *x = sinf(3.0f * t); *y = sinf(2.0f * t); break;
        case 2: *x = cosf(t) + 0.5f * cosf(3.0f * t);
                *y = sinf(t) - 0.5f * sinf(3.0f * t); break;
        default:
            *x = 0.1f * t * cosf(t);
            *y = 0.1f * t * sinf(t);
            break;
    }
}

static float eval_polar(int idx, float t) {
    switch (idx) {
        case 0: return cosf(4.0f * t);
        case 1: return 0.2f * t;
        case 2: return 1.0f - cosf(t);
        default: {
            float v = cosf(2.0f * t);
            if (v < 0.0f) v = -v;
            return sqrtf(v);
        }
    }
}

static bool draw_seg_button(Rectangle bounds, const char *label, bool active) {
    Vector2 mouse = ui_mouse();
    bool hovered = CheckCollisionPointRec(mouse, bounds);
    Color bg = active ? COL_ACCENT : (hovered ? (Color){50, 52, 62, 255} : COL_TAB);
    DrawRectangleRounded(bounds, 0.3f, 6, bg);
    int tw = ui_measure_text(label, FONT_SIZE_SMALL);
    ui_draw_text(label,
                 (int)(bounds.x + (bounds.width - tw) / 2),
                 (int)(bounds.y + (bounds.height - FONT_SIZE_SMALL) / 2),
                 FONT_SIZE_SMALL, active ? WHITE : COL_TEXT_DIM);
    return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static bool draw_small_btn(Rectangle bounds, const char *label) {
    Vector2 mouse = ui_mouse();
    bool hovered = CheckCollisionPointRec(mouse, bounds);
    Color bg = hovered ? COL_TAB_ACT : COL_TAB;
    DrawRectangleRounded(bounds, 0.25f, 6, bg);
    int tw = ui_measure_text(label, FONT_SIZE_SMALL);
    ui_draw_text(label,
                 (int)(bounds.x + (bounds.width - tw) / 2),
                 (int)(bounds.y + (bounds.height - FONT_SIZE_SMALL) / 2),
                 FONT_SIZE_SMALL, COL_TEXT);
    return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static void mathsim_init(void) {
    math_mode = MATH_PARAM;
    param_idx = 0;
    polar_idx = 0;
    zoom = 1.0f;
}

static void mathsim_update(Rectangle area) {
    Rectangle panel = {0};
    Rectangle plot = {0};
    math_layout(area, &panel, &plot, NULL);
    (void)panel;

    Vector2 mouse = ui_mouse();
    if (CheckCollisionPointRec(mouse, plot)) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            zoom *= (wheel > 0) ? 1.1f : (1.0f / 1.1f);
            if (zoom < 0.4f) zoom = 0.4f;
            if (zoom > 4.0f) zoom = 4.0f;
        }
    }
    if (IsKeyPressed(KEY_HOME)) zoom = 1.0f;
}

static void mathsim_draw(Rectangle area) {
    Rectangle panel = {0};
    Rectangle plot = {0};
    bool side_by_side = true;
    math_layout(area, &panel, &plot, &side_by_side);

    // Panel
    DrawRectangleRec(panel, COL_PANEL);
    if (side_by_side) {
        DrawLine((int)(panel.x + panel.width), (int)panel.y,
                 (int)(panel.x + panel.width), (int)(panel.y + panel.height), COL_GRID);
    } else {
        DrawLine((int)panel.x, (int)(panel.y + panel.height),
                 (int)(panel.x + panel.width), (int)(panel.y + panel.height), COL_GRID);
    }

    float sx = panel.x + 8;
    float sw = panel.width - 16;
    float sy = panel.y + 8;

    ui_draw_text("Math Simulations", (int)sx, (int)sy, FONT_SIZE_LARGE, COL_ACCENT);
    sy += 32;

    // Mode toggle
    Rectangle toggle = { sx, sy, sw, 28 };
    Rectangle left = { toggle.x, toggle.y, toggle.width / 2 - 2, toggle.height };
    Rectangle right = { toggle.x + toggle.width / 2 + 2, toggle.y, toggle.width / 2 - 2, toggle.height };
    if (draw_seg_button(left, "Parametric", math_mode == MATH_PARAM)) math_mode = MATH_PARAM;
    if (draw_seg_button(right, "Polar", math_mode == MATH_POLAR)) math_mode = MATH_POLAR;
    sy += 36;

    // Preset selector
    ui_draw_text("Preset", (int)sx, (int)sy, FONT_SIZE_SMALL, COL_TEXT_DIM);
    sy += 18;

    Rectangle btn_prev = { sx, sy, 28, 26 };
    Rectangle btn_next = { sx + sw - 28, sy, 28, 26 };
    if (draw_small_btn(btn_prev, "<")) {
        if (math_mode == MATH_PARAM) param_idx = (param_idx - 1 + PARAM_COUNT) % PARAM_COUNT;
        else polar_idx = (polar_idx - 1 + POLAR_COUNT) % POLAR_COUNT;
    }
    if (draw_small_btn(btn_next, ">")) {
        if (math_mode == MATH_PARAM) param_idx = (param_idx + 1) % PARAM_COUNT;
        else polar_idx = (polar_idx + 1) % POLAR_COUNT;
    }

    const char *preset_name = (math_mode == MATH_PARAM)
        ? param_presets[param_idx].name
        : polar_presets[polar_idx].name;
    int pnw = ui_measure_text(preset_name, FONT_SIZE_SMALL);
    ui_draw_text(preset_name,
                 (int)(sx + (sw - pnw) / 2),
                 (int)(sy + 4), FONT_SIZE_SMALL, COL_TEXT);
    sy += 36;

    if (math_mode == MATH_PARAM) {
        const ParamPreset *p = &param_presets[param_idx];
        char line1[128], line2[128];
        snprintf(line1, sizeof(line1), "x(t) = %s", p->eqx);
        snprintf(line2, sizeof(line2), "y(t) = %s", p->eqy);
        ui_draw_text(line1, (int)sx, (int)sy, FONT_SIZE_SMALL, COL_TEXT_DIM);
        sy += 18;
        ui_draw_text(line2, (int)sx, (int)sy, FONT_SIZE_SMALL, COL_TEXT_DIM);
        sy += 18;
    } else {
        const PolarPreset *p = &polar_presets[polar_idx];
        char line1[128];
        snprintf(line1, sizeof(line1), "%s", p->eq);
        ui_draw_text(line1, (int)sx, (int)sy, FONT_SIZE_SMALL, COL_TEXT_DIM);
        sy += 18;
    }

    char zoom_label[64];
    snprintf(zoom_label, sizeof(zoom_label), "Zoom: %.2fx (scroll to zoom)", zoom);
    ui_draw_text(zoom_label, (int)sx, (int)(panel.y + panel.height - 20),
                 FONT_SIZE_TINY, COL_TEXT_DIM);

    // Plot area
    DrawRectangleRec(plot, COL_BG);
    ui_scissor_begin(plot.x, plot.y, plot.width, plot.height);

    float base = fminf(plot.width, plot.height) * 0.4f;
    float scale = (base / 5.0f) * zoom;

    draw_grid(plot, scale);

    // Draw curve
    Vector2 prev = {0};
    bool has_prev = false;
    if (math_mode == MATH_PARAM) {
        const ParamPreset *p = &param_presets[param_idx];
        for (float t = p->tmin; t <= p->tmax; t += p->step) {
            float x, y;
            eval_param(param_idx, t, &x, &y);
            Vector2 sp = math_to_screen(plot, scale, x, y);
            if (has_prev) DrawLineV(prev, sp, COL_ACCENT2);
            prev = sp;
            has_prev = true;
        }
    } else {
        const PolarPreset *p = &polar_presets[polar_idx];
        for (float t = p->tmin; t <= p->tmax; t += p->step) {
            float r = eval_polar(polar_idx, t);
            if (!isfinite(r)) continue;
            float x = r * cosf(t);
            float y = r * sinf(t);
            Vector2 sp = math_to_screen(plot, scale, x, y);
            if (has_prev) DrawLineV(prev, sp, COL_ACCENT2);
            prev = sp;
            has_prev = true;
        }
    }

    EndScissorMode();
}

static void mathsim_cleanup(void) {
    // Nothing to free
}

static Module mathsim_mod = {
    .name    = "Math Sims",
    .init    = mathsim_init,
    .update  = mathsim_update,
    .draw    = mathsim_draw,
    .cleanup = mathsim_cleanup,
};

Module *mathsim_module(void) {
    return &mathsim_mod;
}
