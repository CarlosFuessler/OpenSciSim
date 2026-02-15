#include "mechanics.h"
#include "../../ui/ui.h"
#include "../../ui/theme.h"
#include <math.h>
#include <stdio.h>

typedef enum {
    MECH_PENDULUM,
    MECH_PROJECTILE
} MechMode;

static int   mech_mode = MECH_PENDULUM;

static float pendulum_length = 2.0f;
static float pendulum_angle_deg = 30.0f;
static float pendulum_g = 9.81f;
static float pendulum_time = 0.0f;
static bool  pendulum_running = true;

static float proj_speed = 25.0f;
static float proj_angle_deg = 45.0f;
static float proj_g = 9.81f;
static float proj_time = 0.0f;
static bool  proj_running = false;

static void mech_layout(Rectangle area, Rectangle *panel, Rectangle *view, bool *side_by_side) {
    float aspect = (float)GetScreenWidth() / (float)GetScreenHeight();
    float gap = 12.0f;
    Rectangle content = ui_pad(area, 10.0f);
    bool side = aspect >= 1.35f;
    float weights_row[2] = {1.1f, 2.5f};
    float weights_col[2] = {1.4f, 2.6f};

    if (side) {
        *panel = ui_layout_row(content, 2, 0, gap, weights_row);
        *view  = ui_layout_row(content, 2, 1, gap, weights_row);
    } else {
        *panel = ui_layout_col(content, 2, 0, gap, weights_col);
        *view  = ui_layout_col(content, 2, 1, gap, weights_col);
    }
    if (side_by_side) *side_by_side = side;
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

static bool draw_btn(Rectangle bounds, const char *label) {
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

static void draw_param(const char *label, float *value, float step,
                       float min_v, float max_v, float x, float *y, float w,
                       const char *suffix) {
    float row_h = 26;
    ui_draw_text(label, (int)x, (int)(*y + 4), FONT_SIZE_SMALL, COL_TEXT_DIM);

    char vbuf[32];
    if (suffix)
        snprintf(vbuf, sizeof(vbuf), "%.2f%s", *value, suffix);
    else
        snprintf(vbuf, sizeof(vbuf), "%.2f", *value);

    float btn = 22;
    Rectangle minus = { x + w - btn * 2 - 6, *y, btn, btn };
    Rectangle plus  = { x + w - btn, *y, btn, btn };

    int vw = ui_measure_text(vbuf, FONT_SIZE_SMALL);
    float vx = x + w - btn * 2 - 12 - vw;
    if (vx < x + 80) vx = x + 80;
    ui_draw_text(vbuf, (int)vx, (int)(*y + 4), FONT_SIZE_SMALL, COL_TEXT);

    if (draw_btn(minus, "-")) {
        *value -= step;
        if (*value < min_v) *value = min_v;
    }
    if (draw_btn(plus, "+")) {
        *value += step;
        if (*value > max_v) *value = max_v;
    }

    *y += row_h + 6;
}

static void mechanics_init(void) {
    mech_mode = MECH_PENDULUM;
    pendulum_time = 0.0f;
    proj_time = 0.0f;
}

static void mechanics_update(Rectangle area) {
    Rectangle panel = {0};
    Rectangle view = {0};
    mech_layout(area, &panel, &view, NULL);
    (void)panel;

    float dt = GetFrameTime();
    if (mech_mode == MECH_PENDULUM) {
        if (pendulum_running) pendulum_time += dt;
    } else {
        if (proj_running) {
            proj_time += dt;
            float v = proj_speed;
            float g = proj_g;
            float a = proj_angle_deg * DEG2RAD;
            float tmax = (g > 0.0f) ? (2.0f * v * sinf(a) / g) : 0.0f;
            if (proj_time > tmax) proj_running = false;
        }
    }
}

static void draw_pendulum(Rectangle view) {
    DrawRectangleRec(view, COL_BG);
    ui_scissor_begin(view.x, view.y, view.width, view.height);

    float L = pendulum_length;
    float g = pendulum_g;
    float theta0 = pendulum_angle_deg * DEG2RAD;
    float omega = (g > 0.0f) ? sqrtf(g / L) : 0.0f;
    float theta = theta0 * cosf(omega * pendulum_time);

    float max_len = L + 0.6f;
    float scale = (view.height - 100.0f) / max_len;
    if (scale < 40.0f) scale = 40.0f;

    Vector2 pivot = { view.x + view.width * 0.5f, view.y + 40.0f };
    Vector2 bob = {
        pivot.x + sinf(theta) * L * scale,
        pivot.y + cosf(theta) * L * scale
    };

    DrawLineV(pivot, bob, COL_TEXT_DIM);
    DrawCircleV(pivot, 4, COL_ACCENT);
    DrawCircleV(bob, 12, COL_ACCENT2);
    DrawCircleLines((int)bob.x, (int)bob.y, 12, WHITE);

    EndScissorMode();
}

static void draw_projectile(Rectangle view) {
    DrawRectangleRec(view, COL_BG);
    ui_scissor_begin(view.x, view.y, view.width, view.height);

    float v = proj_speed;
    float g = proj_g;
    float a = proj_angle_deg * DEG2RAD;
    float range = (g > 0.0f) ? (v * v * sinf(2.0f * a) / g) : 1.0f;
    float max_h = (g > 0.0f) ? (v * v * sinf(a) * sinf(a) / (2.0f * g)) : 1.0f;

    if (range < 1.0f) range = 1.0f;
    if (max_h < 1.0f) max_h = 1.0f;

    float sx = (view.width * 0.8f) / range;
    float sy = (view.height * 0.7f) / max_h;
    float scale = fminf(sx, sy);

    Vector2 origin = { view.x + 40.0f, view.y + view.height - 30.0f };
    DrawLine((int)origin.x, (int)origin.y, (int)(view.x + view.width - 20), (int)origin.y, COL_GRID);

    // Path
    float tmax = (g > 0.0f) ? (2.0f * v * sinf(a) / g) : 0.0f;
    Vector2 prev = origin;
    bool has_prev = false;
    for (int i = 0; i <= 200; i++) {
        float t = tmax * ((float)i / 200.0f);
        float x = v * cosf(a) * t;
        float y = v * sinf(a) * t - 0.5f * g * t * t;
        if (y < 0.0f) y = 0.0f;
        Vector2 p = { origin.x + x * scale, origin.y - y * scale };
        if (has_prev) DrawLineV(prev, p, (Color){80, 160, 255, 180});
        prev = p;
        has_prev = true;
    }

    // Current projectile
    float t = proj_running ? proj_time : 0.0f;
    float x = v * cosf(a) * t;
    float y = v * sinf(a) * t - 0.5f * g * t * t;
    if (y < 0.0f) y = 0.0f;
    Vector2 p = { origin.x + x * scale, origin.y - y * scale };
    DrawCircleV(p, 6, COL_ACCENT);

    EndScissorMode();
}

static void mechanics_draw(Rectangle area) {
    Rectangle panel = {0};
    Rectangle view = {0};
    bool side_by_side = true;
    mech_layout(area, &panel, &view, &side_by_side);

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

    ui_draw_text("Physics Mechanics", (int)sx, (int)sy, FONT_SIZE_LARGE, COL_ACCENT);
    sy += 32;

    Rectangle toggle = { sx, sy, sw, 28 };
    Rectangle left = { toggle.x, toggle.y, toggle.width / 2 - 2, toggle.height };
    Rectangle right = { toggle.x + toggle.width / 2 + 2, toggle.y, toggle.width / 2 - 2, toggle.height };
    if (draw_seg_button(left, "Pendulum", mech_mode == MECH_PENDULUM)) mech_mode = MECH_PENDULUM;
    if (draw_seg_button(right, "Projectile", mech_mode == MECH_PROJECTILE)) mech_mode = MECH_PROJECTILE;
    sy += 36;

    if (mech_mode == MECH_PENDULUM) {
        draw_param("Length (m)", &pendulum_length, 0.1f, 0.6f, 5.0f, sx, &sy, sw, "");
        draw_param("Amplitude", &pendulum_angle_deg, 2.0f, 5.0f, 80.0f, sx, &sy, sw, "°");
        draw_param("Gravity", &pendulum_g, 0.2f, 1.0f, 20.0f, sx, &sy, sw, " m/s²");

        Rectangle btn = { sx, sy, sw, 28 };
        const char *label = pendulum_running ? "Pause" : "Start";
        if (draw_btn(btn, label)) pendulum_running = !pendulum_running;
        sy += 36;

        if (draw_btn((Rectangle){sx, sy, sw, 28}, "Reset")) {
            pendulum_time = 0.0f;
            pendulum_running = true;
        }
    } else {
        draw_param("Speed", &proj_speed, 1.0f, 5.0f, 60.0f, sx, &sy, sw, " m/s");
        draw_param("Angle", &proj_angle_deg, 2.0f, 10.0f, 80.0f, sx, &sy, sw, "°");
        draw_param("Gravity", &proj_g, 0.2f, 1.0f, 20.0f, sx, &sy, sw, " m/s²");

        Rectangle btn = { sx, sy, sw, 28 };
        if (draw_btn(btn, "Launch")) {
            proj_time = 0.0f;
            proj_running = true;
        }
        sy += 36;
        if (draw_btn((Rectangle){sx, sy, sw, 28}, "Reset")) {
            proj_time = 0.0f;
            proj_running = false;
        }
    }

    // Right view
    if (mech_mode == MECH_PENDULUM)
        draw_pendulum(view);
    else
        draw_projectile(view);
}

static void mechanics_cleanup(void) {
    // Nothing to free
}

static Module mechanics_mod = {
    .name    = "Mechanics",
    .init    = mechanics_init,
    .update  = mechanics_update,
    .draw    = mechanics_draw,
    .cleanup = mechanics_cleanup,
};

Module *mechanics_module(void) {
    return &mechanics_mod;
}
