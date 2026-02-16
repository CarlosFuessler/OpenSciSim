#include "chemsim.h"
#include "../../ui/ui.h"
#include "../../ui/theme.h"
#include <math.h>
#include <stdio.h>

typedef enum {
    CHEM_REACTION,
    CHEM_ACIDBASE
} ChemMode;

typedef struct {
    Vector2 pos;
    Vector2 vel;
    int type; // 0=A, 1=B, 2=Product
} Particle;

#define MAX_PARTICLES 48

static ChemMode chem_mode = CHEM_REACTION;
static Particle particles[MAX_PARTICLES];
static int particle_count = 0;
static bool reaction_running = true;
static float temperature = 1.0f;
static Rectangle reaction_bounds = {0};
static bool reaction_ready = false;

static float ph_value = 7.0f;

static void chem_layout(Rectangle area, Rectangle *panel, Rectangle *view, bool *side_by_side) {
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

static Color lerp_color(Color a, Color b, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return (Color){
        (unsigned char)(a.r + (b.r - a.r) * t),
        (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t),
        255
    };
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

static void reset_reaction(Rectangle view) {
    reaction_bounds = view;
    reaction_ready = true;
    particle_count = MAX_PARTICLES;

    for (int i = 0; i < particle_count; i++) {
        float pad = 16.0f;
        float x = view.x + pad + (float)GetRandomValue(0, (int)(view.width - pad * 2));
        float y = view.y + pad + (float)GetRandomValue(0, (int)(view.height - pad * 2));
        float vx = (float)GetRandomValue(-50, 50);
        float vy = (float)GetRandomValue(-50, 50);
        particles[i].pos = (Vector2){x, y};
        particles[i].vel = (Vector2){vx, vy};
        particles[i].type = (i < particle_count / 2) ? 0 : 1;
    }
}

static void update_reaction(Rectangle view) {
    if (!reaction_ready ||
        view.width != reaction_bounds.width ||
        view.height != reaction_bounds.height ||
        view.x != reaction_bounds.x || view.y != reaction_bounds.y) {
        reset_reaction(view);
    }

    float dt = GetFrameTime();
    if (!reaction_running) return;

    float pad = 10.0f;
    float left = view.x + pad;
    float right = view.x + view.width - pad;
    float top = view.y + pad;
    float bottom = view.y + view.height - pad;

    for (int i = 0; i < particle_count; i++) {
        Particle *p = &particles[i];
        p->pos.x += p->vel.x * dt * temperature;
        p->pos.y += p->vel.y * dt * temperature;
        if (p->pos.x < left || p->pos.x > right) p->vel.x *= -1;
        if (p->pos.y < top || p->pos.y > bottom) p->vel.y *= -1;
        if (p->pos.x < left) p->pos.x = left;
        if (p->pos.x > right) p->pos.x = right;
        if (p->pos.y < top) p->pos.y = top;
        if (p->pos.y > bottom) p->pos.y = bottom;
    }

    // Simple A + B -> Product reaction (skip reacted particles early)
    for (int i = 0; i < particle_count; i++) {
        Particle *a = &particles[i];
        if (a->type == 2) continue;
        for (int j = i + 1; j < particle_count; j++) {
            Particle *b = &particles[j];
            if (b->type == 2) continue;
            if (a->type == b->type) continue;
            float dx = a->pos.x - b->pos.x;
            if (dx > 10.0f || dx < -10.0f) continue;
            float dy = a->pos.y - b->pos.y;
            if (dy > 10.0f || dy < -10.0f) continue;
            float dist2 = dx * dx + dy * dy;
            if (dist2 < 100.0f) {
                a->type = 2;
                b->type = 2;
                a->vel.x *= 0.3f;
                a->vel.y *= 0.3f;
                b->vel.x *= 0.3f;
                b->vel.y *= 0.3f;
                break;
            }
        }
    }
}

static void chemsim_init(void) {
    chem_mode = CHEM_REACTION;
    reaction_running = true;
    temperature = 1.0f;
    ph_value = 7.0f;
    reaction_ready = false;
}

static void chemsim_update(Rectangle area) {
    Rectangle panel = {0};
    Rectangle view = {0};
    chem_layout(area, &panel, &view, NULL);
    (void)panel;

    if (chem_mode == CHEM_REACTION) {
        update_reaction(view);
    }
}

static void draw_reaction(Rectangle view) {
    DrawRectangleRec(view, COL_BG);
    ui_scissor_begin(view.x, view.y, view.width, view.height);

    int count_a = 0, count_b = 0, count_c = 0;
    for (int i = 0; i < particle_count; i++) {
        Particle *p = &particles[i];
        Color col = (p->type == 0) ? (Color){255, 120, 80, 220} :
                    (p->type == 1) ? (Color){80, 180, 255, 220} :
                                      (Color){120, 220, 140, 220};
        if (p->type == 0) count_a++;
        else if (p->type == 1) count_b++;
        else count_c++;
        DrawCircleV(p->pos, 6, col);
        DrawCircleLines((int)p->pos.x, (int)p->pos.y, 6, (Color){255,255,255,40});
    }

    EndScissorMode();

    char info[128];
    snprintf(info, sizeof(info), "A: %d   B: %d   Product: %d", count_a, count_b, count_c);
    int tw = ui_measure_text(info, FONT_SIZE_SMALL);
    float ox = view.x + (view.width - tw) / 2;
    DrawRectangleRounded(
        (Rectangle){ox - 8, view.y + 10, (float)(tw + 16), (float)(FONT_SIZE_SMALL + 6)},
        0.3f, 6, (Color){COL_PANEL.r, COL_PANEL.g, COL_PANEL.b, 200}
    );
    ui_draw_text(info, (int)ox, (int)(view.y + 12), FONT_SIZE_SMALL, COL_TEXT);
}

static void draw_acidbase(Rectangle view) {
    DrawRectangleRec(view, COL_BG);
    ui_scissor_begin(view.x, view.y, view.width, view.height);

    Color acid = (Color){230, 70, 70, 255};
    Color neutral = (Color){80, 200, 120, 255};
    Color base = (Color){80, 120, 230, 255};
    Color col;
    if (ph_value <= 7.0f) {
        col = lerp_color(acid, neutral, ph_value / 7.0f);
    } else {
        col = lerp_color(neutral, base, (ph_value - 7.0f) / 7.0f);
    }

    float bar_w = view.width * 0.8f;
    float bar_h = 18;
    float bx = view.x + (view.width - bar_w) / 2;
    float by = view.y + 40;
    DrawRectangleGradientH((int)bx, (int)by, (int)(bar_w * 0.5f), (int)bar_h, acid, neutral);
    DrawRectangleGradientH((int)(bx + bar_w * 0.5f), (int)by, (int)(bar_w * 0.5f), (int)bar_h, neutral, base);

    float marker_x = bx + (ph_value / 14.0f) * bar_w;
    DrawLine((int)marker_x, (int)by - 4, (int)marker_x, (int)by + bar_h + 4, WHITE);

    char label[64];
    snprintf(label, sizeof(label), "pH %.1f", ph_value);
    int tw = ui_measure_text(label, FONT_SIZE_DEFAULT);
    ui_draw_text(label, (int)(view.x + (view.width - tw) / 2), (int)(by + 30), FONT_SIZE_DEFAULT, col);

    char desc[64];
    if (ph_value < 7.0f) snprintf(desc, sizeof(desc), "Acidic");
    else if (ph_value > 7.0f) snprintf(desc, sizeof(desc), "Basic");
    else snprintf(desc, sizeof(desc), "Neutral");
    int dw = ui_measure_text(desc, FONT_SIZE_SMALL);
    ui_draw_text(desc, (int)(view.x + (view.width - dw) / 2), (int)(by + 56), FONT_SIZE_SMALL, COL_TEXT_DIM);

    double h = pow(10.0, -ph_value);
    char info[64];
    snprintf(info, sizeof(info), "[H+] ≈ %.1e mol/L", h);
    int iw = ui_measure_text(info, FONT_SIZE_SMALL);
    ui_draw_text(info, (int)(view.x + (view.width - iw) / 2), (int)(by + 76), FONT_SIZE_SMALL, COL_TEXT_DIM);

    DrawRectangleRounded(
        (Rectangle){view.x + view.width * 0.5f - 60, view.y + view.height - 120, 120, 120},
        0.4f, 6, (Color){col.r, col.g, col.b, 200}
    );
    DrawRectangleRoundedLinesEx(
        (Rectangle){view.x + view.width * 0.5f - 60, view.y + view.height - 120, 120, 120},
        0.4f, 6, 1.5f, WHITE
    );

    EndScissorMode();
}

static void chemsim_draw(Rectangle area) {
    Rectangle panel = {0};
    Rectangle view = {0};
    bool side_by_side = true;
    chem_layout(area, &panel, &view, &side_by_side);

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

    ui_draw_text("Chemistry Lab", (int)sx, (int)sy, FONT_SIZE_LARGE, COL_ACCENT);
    sy += 32;

    Rectangle toggle = { sx, sy, sw, 28 };
    Rectangle left = { toggle.x, toggle.y, toggle.width / 2 - 2, toggle.height };
    Rectangle right = { toggle.x + toggle.width / 2 + 2, toggle.y, toggle.width / 2 - 2, toggle.height };
    if (draw_seg_button(left, "Reaction", chem_mode == CHEM_REACTION)) chem_mode = CHEM_REACTION;
    if (draw_seg_button(right, "Acid/Base", chem_mode == CHEM_ACIDBASE)) chem_mode = CHEM_ACIDBASE;
    sy += 36;

    if (chem_mode == CHEM_REACTION) {
        draw_param("Temperature", &temperature, 0.1f, 0.5f, 2.0f, sx, &sy, sw, "x");
        Rectangle btn = { sx, sy, sw, 28 };
        const char *label = reaction_running ? "Pause" : "Start";
        if (draw_btn(btn, label)) reaction_running = !reaction_running;
        sy += 36;
        if (draw_btn((Rectangle){sx, sy, sw, 28}, "Reset")) reset_reaction(view);
        draw_reaction(view);
    } else {
        draw_param("pH", &ph_value, 0.2f, 0.0f, 14.0f, sx, &sy, sw, "");
        if (draw_btn((Rectangle){sx, sy, sw, 28}, "Neutral (pH 7)")) ph_value = 7.0f;
        draw_acidbase(view);
    }
}

static void chemsim_cleanup(void) {
    // Nothing to free
}

static Module chemsim_mod = {
    .name    = "Chemistry Lab",
    .help_text = "Reaction: Particles A + B collide to form Products.\n"
                 "  Temperature controls particle speed.\n"
                 "  Start/Pause toggles simulation, Reset respawns particles.\n"
                 "Acid/Base: Adjust pH (0-14) to see [H+] concentration.\n"
                 "Press [H] to toggle this help.",
    .init    = chemsim_init,
    .update  = chemsim_update,
    .draw    = chemsim_draw,
    .cleanup = chemsim_cleanup,
};

Module *chemsim_module(void) {
    return &chemsim_mod;
}
