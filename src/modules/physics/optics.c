#include "optics.h"
#include "../../ui/ui.h"
#include "../../ui/theme.h"
#include <math.h>
#include <stdio.h>

/* ── modes ── */
typedef enum {
    OPTICS_PHOTON,
    OPTICS_DIFFRACTION
} OpticsMode;

static int optics_mode = OPTICS_PHOTON;

/* ── optical element types ── */
typedef enum {
    ELEM_MIRROR,
    ELEM_GLASS,
    ELEM_PRISM,
    ELEM_LENS,
    ELEM_COUNT
} ElemType;

static const char *elem_names[ELEM_COUNT] = {
    "Mirror", "Glass", "Prism", "Lens"
};

/* ── photon sim state ── */
#define MAX_PHOTONS   120
#define MAX_ELEMENTS    6
#define PHOTON_SPEED 280.0f
#define GLASS_IOR     1.5f

typedef struct { Vector2 pos; Vector2 dir; bool active; Color col; } Photon;
typedef struct {
    ElemType type;
    Vector2  a, b;       /* line segment (mirror/glass face) */
    Vector2  center;     /* for prism/lens */
    float    size;       /* for prism/lens */
} OptElem;

static Photon   photons[MAX_PHOTONS];
static int      photon_count = 0;
static OptElem  elements[MAX_ELEMENTS];
static int      elem_count = 0;
static int      active_elem_type = ELEM_MIRROR;
static float    emitter_angle = 0.0f;
static float    emit_timer = 0.0f;
static float    wavelength = 550.0f;   /* nm – maps to visible colour */
static bool     photon_running = true;
static Rectangle photon_bounds = {0};
static bool     photon_ready = false;

/* ── diffraction state ── */
static int   diff_slits       = 2;       /* 1 = single, 2 = double */
static float slit_width       = 40.0f;   /* µm (display units) */
static float slit_sep         = 120.0f;  /* µm, only for double */
static float diff_wavelength  = 550.0f;  /* nm */

/* ── helpers ── */
static void optics_layout(Rectangle area, Rectangle *panel, Rectangle *view, bool *side_by_side) {
    float aspect = (float)GetScreenWidth() / (float)GetScreenHeight();
    float gap = 12.0f;
    Rectangle content = ui_pad(area, 10.0f);
    bool side = aspect >= 1.35f;
    float wr[2] = {1.1f, 2.5f};
    float wc[2] = {1.4f, 2.6f};
    if (side) {
        *panel = ui_layout_row(content, 2, 0, gap, wr);
        *view  = ui_layout_row(content, 2, 1, gap, wr);
    } else {
        *panel = ui_layout_col(content, 2, 0, gap, wc);
        *view  = ui_layout_col(content, 2, 1, gap, wc);
    }
    if (side_by_side) *side_by_side = side;
}

static bool draw_seg_button(Rectangle bounds, const char *label, bool active) {
    Vector2 mouse = ui_mouse();
    bool hovered = CheckCollisionPointRec(mouse, bounds);
    Color bg = active ? COL_ACCENT : (hovered ? (Color){50,52,62,255} : COL_TAB);
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
    if (suffix) snprintf(vbuf, sizeof(vbuf), "%.0f%s", *value, suffix);
    else        snprintf(vbuf, sizeof(vbuf), "%.0f", *value);
    float btn = 22;
    Rectangle minus = { x + w - btn*2 - 6, *y, btn, btn };
    Rectangle plus  = { x + w - btn,       *y, btn, btn };
    int vw = ui_measure_text(vbuf, FONT_SIZE_SMALL);
    float vx = x + w - btn*2 - 12 - vw;
    if (vx < x + 80) vx = x + 80;
    ui_draw_text(vbuf, (int)vx, (int)(*y + 4), FONT_SIZE_SMALL, COL_TEXT);
    if (draw_btn(minus, "-")) { *value -= step; if (*value < min_v) *value = min_v; }
    if (draw_btn(plus,  "+")) { *value += step; if (*value > max_v) *value = max_v; }
    *y += row_h + 6;
}

/* ── wavelength → visible colour ── */
static Color wavelength_to_color(float nm) {
    float r = 0, g = 0, b = 0;
    if      (nm < 380) { r=0.4f; g=0;   b=0.4f; }
    else if (nm < 440) { r=(440-nm)/(440-380); b=1; }
    else if (nm < 490) { g=(nm-440)/(490-440); b=1; }
    else if (nm < 510) { g=1; b=(510-nm)/(510-490); }
    else if (nm < 580) { r=(nm-510)/(580-510); g=1; }
    else if (nm < 645) { r=1; g=(645-nm)/(645-580); }
    else               { r=1; }
    /* intensity fall-off at edges */
    float f = 1.0f;
    if (nm < 420) f = 0.3f + 0.7f*(nm-380)/40.0f;
    else if (nm > 700) f = 0.3f + 0.7f*(780-nm)/80.0f;
    if (f < 0) f = 0;
    return (Color){
        (unsigned char)(r*f*255), (unsigned char)(g*f*255),
        (unsigned char)(b*f*255), 220
    };
}

/* ── photon sim ── */
static void setup_elements(Rectangle view) {
    float cx = view.x + view.width * 0.5f;
    float cy = view.y + view.height * 0.5f;
    float s  = fminf(view.width, view.height) * 0.35f;

    elem_count = 0;
    if (active_elem_type == ELEM_MIRROR) {
        elements[elem_count++] = (OptElem){
            .type = ELEM_MIRROR,
            .a = {cx + s*0.4f, cy - s*0.5f},
            .b = {cx + s*0.4f, cy + s*0.4f}
        };
        elements[elem_count++] = (OptElem){
            .type = ELEM_MIRROR,
            .a = {cx - s*0.1f, cy + s*0.45f},
            .b = {cx + s*0.5f, cy + s*0.45f}
        };
    } else if (active_elem_type == ELEM_GLASS) {
        /* two parallel faces of a glass block */
        float bw = s * 0.3f;
        elements[elem_count++] = (OptElem){
            .type = ELEM_GLASS,
            .a = {cx - bw, cy - s*0.4f}, .b = {cx - bw, cy + s*0.4f}
        };
        elements[elem_count++] = (OptElem){
            .type = ELEM_GLASS,
            .a = {cx + bw, cy - s*0.4f}, .b = {cx + bw, cy + s*0.4f}
        };
    } else if (active_elem_type == ELEM_PRISM) {
        elements[elem_count++] = (OptElem){
            .type = ELEM_PRISM, .center = {cx, cy}, .size = s * 0.55f
        };
    } else if (active_elem_type == ELEM_LENS) {
        elements[elem_count++] = (OptElem){
            .type = ELEM_LENS, .center = {cx + s*0.1f, cy}, .size = s * 0.6f
        };
    }
}

static void reset_photon_sim(Rectangle view) {
    photon_bounds = view;
    photon_ready  = true;
    photon_count  = 0;
    emit_timer    = 0;
    setup_elements(view);
}

static float vec2_dot(Vector2 a, Vector2 b) { return a.x*b.x + a.y*b.y; }

static float vec2_len(Vector2 v) { return sqrtf(v.x*v.x + v.y*v.y); }

/* refract direction through surface normal; returns false for total internal reflection */
static bool refract_dir(Vector2 dir, Vector2 n, float n1, float n2, Vector2 *out) {
    float cos_i = -vec2_dot(dir, n);
    if (cos_i < 0) { n.x = -n.x; n.y = -n.y; cos_i = -cos_i; float tmp = n1; n1 = n2; n2 = tmp; }
    float ratio = n1 / n2;
    float sin2_t = ratio * ratio * (1.0f - cos_i * cos_i);
    if (sin2_t > 1.0f) return false; /* total internal reflection */
    float cos_t = sqrtf(1.0f - sin2_t);
    out->x = ratio * dir.x + (ratio * cos_i - cos_t) * n.x;
    out->y = ratio * dir.y + (ratio * cos_i - cos_t) * n.y;
    float l = vec2_len(*out);
    if (l > 0) { out->x /= l; out->y /= l; }
    return true;
}

static void interact_with_elements(Photon *p) {
    for (int e = 0; e < elem_count; e++) {
        OptElem *el = &elements[e];

        if (el->type == ELEM_MIRROR || el->type == ELEM_GLASS) {
            Vector2 edge = {el->b.x - el->a.x, el->b.y - el->a.y};
            float len = vec2_len(edge);
            if (len < 1.0f) continue;
            Vector2 n = {-edge.y / len, edge.x / len};
            Vector2 ap = {p->pos.x - el->a.x, p->pos.y - el->a.y};
            float d = vec2_dot(ap, n);
            if (fabsf(d) > 4.0f) continue;
            float proj = vec2_dot(ap, (Vector2){edge.x/len, edge.y/len});
            if (proj < 0 || proj > len) continue;

            if (el->type == ELEM_MIRROR) {
                float dn = vec2_dot(p->dir, n);
                p->dir.x -= 2*dn*n.x;
                p->dir.y -= 2*dn*n.y;
            } else {
                /* glass: refract */
                Vector2 refr;
                if (!refract_dir(p->dir, n, 1.0f, GLASS_IOR, &refr)) {
                    /* total internal reflection */
                    float dn = vec2_dot(p->dir, n);
                    p->dir.x -= 2*dn*n.x;
                    p->dir.y -= 2*dn*n.y;
                } else {
                    p->dir = refr;
                }
            }
            p->pos.x += n.x * 5.0f * (d < 0 ? -1 : 1);
            p->pos.y += n.y * 5.0f * (d < 0 ? -1 : 1);
            break;
        }
        else if (el->type == ELEM_PRISM) {
            /* equilateral triangle prism – 3 faces */
            float r = el->size * 0.5f;
            Vector2 c = el->center;
            Vector2 tri[3] = {
                {c.x, c.y - r},
                {c.x - r * 0.866f, c.y + r * 0.5f},
                {c.x + r * 0.866f, c.y + r * 0.5f}
            };
            for (int f = 0; f < 3; f++) {
                Vector2 fa = tri[f], fb = tri[(f+1)%3];
                Vector2 edge = {fb.x - fa.x, fb.y - fa.y};
                float len = vec2_len(edge);
                if (len < 1.0f) continue;
                Vector2 n = {-edge.y / len, edge.x / len};
                Vector2 ap = {p->pos.x - fa.x, p->pos.y - fa.y};
                float d = vec2_dot(ap, n);
                if (fabsf(d) > 4.0f) continue;
                float proj = vec2_dot(ap, (Vector2){edge.x/len, edge.y/len});
                if (proj < 0 || proj > len) continue;

                Vector2 refr;
                if (!refract_dir(p->dir, n, 1.0f, GLASS_IOR, &refr)) {
                    float dn = vec2_dot(p->dir, n);
                    p->dir.x -= 2*dn*n.x;
                    p->dir.y -= 2*dn*n.y;
                } else {
                    p->dir = refr;
                    /* dispersion: slightly shift direction based on wavelength */
                    float disp = (wavelength - 550.0f) / 2000.0f;
                    p->dir.x += n.x * disp;
                    p->dir.y += n.y * disp;
                    float l = vec2_len(p->dir);
                    if (l > 0) { p->dir.x /= l; p->dir.y /= l; }
                }
                p->pos.x += n.x * 5.0f * (d < 0 ? -1 : 1);
                p->pos.y += n.y * 5.0f * (d < 0 ? -1 : 1);
                break;
            }
        }
        else if (el->type == ELEM_LENS) {
            /* thin converging lens: vertical line that bends rays toward focal point */
            float half_h = el->size * 0.5f;
            float lx = el->center.x;
            float ly = el->center.y;
            float d = p->pos.x - lx;
            if (fabsf(d) > 4.0f) continue;
            if (p->pos.y < ly - half_h || p->pos.y > ly + half_h) continue;

            float focal = el->size * 0.8f;
            /* bend toward focal point on the exit side */
            float sign = (p->dir.x > 0) ? 1.0f : -1.0f;
            Vector2 fp = {lx + sign * focal, ly};
            Vector2 to_f = {fp.x - p->pos.x, fp.y - p->pos.y};
            float l = vec2_len(to_f);
            if (l > 1.0f) { p->dir.x = to_f.x / l; p->dir.y = to_f.y / l; }
            p->pos.x = lx + sign * 5.0f;
            break;
        }
    }
}

static void update_photon_sim(Rectangle view) {
    if (!photon_ready ||
        view.x != photon_bounds.x || view.y != photon_bounds.y ||
        view.width != photon_bounds.width || view.height != photon_bounds.height) {
        reset_photon_sim(view);
    }
    if (!photon_running) return;

    float dt = GetFrameTime();
    emit_timer += dt;

    /* emit photons from left edge */
    if (emit_timer > 0.04f && photon_count < MAX_PHOTONS) {
        emit_timer = 0;
        float ang = emitter_angle * DEG2RAD;
        float cy = view.y + view.height * 0.5f;
        photons[photon_count++] = (Photon){
            .pos    = {view.x + 12, cy},
            .dir    = {cosf(ang), sinf(ang)},
            .active = true,
            .col    = wavelength_to_color(wavelength)
        };
    }

    for (int i = 0; i < photon_count; i++) {
        Photon *p = &photons[i];
        if (!p->active) continue;
        p->pos.x += p->dir.x * PHOTON_SPEED * dt;
        p->pos.y += p->dir.y * PHOTON_SPEED * dt;
        interact_with_elements(p);
        /* deactivate if out of view */
        if (p->pos.x < view.x || p->pos.x > view.x + view.width ||
            p->pos.y < view.y || p->pos.y > view.y + view.height)
            p->active = false;
    }

    /* compact dead photons when buffer full */
    if (photon_count >= MAX_PHOTONS) {
        int dst = 0;
        for (int i = 0; i < photon_count; i++)
            if (photons[i].active) photons[dst++] = photons[i];
        photon_count = dst;
    }
}

static void draw_photon_sim(Rectangle view) {
    DrawRectangleRec(view, COL_BG);
    ui_scissor_begin(view.x, view.y, view.width, view.height);

    /* draw optical elements */
    for (int e = 0; e < elem_count; e++) {
        OptElem *el = &elements[e];
        if (el->type == ELEM_MIRROR) {
            DrawLineEx(el->a, el->b, 3.0f, COL_TEXT);
            /* mirror backing lines */
            Vector2 edge = {el->b.x - el->a.x, el->b.y - el->a.y};
            float len = vec2_len(edge);
            if (len > 1.0f) {
                Vector2 n = {-edge.y / len, edge.x / len};
                int segs = (int)(len / 8);
                for (int s = 0; s < segs; s++) {
                    float t = (float)s / segs;
                    Vector2 p0 = {el->a.x + edge.x*t, el->a.y + edge.y*t};
                    Vector2 p1 = {p0.x + n.x*6 + edge.x/len*6, p0.y + n.y*6 + edge.y/len*6};
                    DrawLineV(p0, p1, COL_TEXT_DIM);
                }
            }
        } else if (el->type == ELEM_GLASS) {
            DrawLineEx(el->a, el->b, 3.0f, (Color){80, 180, 255, 200});
            /* if there's a pair, draw the block fill */
            if (e + 1 < elem_count && elements[e+1].type == ELEM_GLASS) {
                OptElem *el2 = &elements[e+1];
                DrawRectangle(
                    (int)fminf(el->a.x, el2->a.x), (int)fminf(el->a.y, el2->a.y),
                    (int)fabsf(el2->a.x - el->a.x), (int)fabsf(el->b.y - el->a.y),
                    (Color){80, 180, 255, 40});
            }
        } else if (el->type == ELEM_PRISM) {
            float r = el->size * 0.5f;
            Vector2 c = el->center;
            Vector2 tri[3] = {
                {c.x, c.y - r},
                {c.x - r * 0.866f, c.y + r * 0.5f},
                {c.x + r * 0.866f, c.y + r * 0.5f}
            };
            DrawTriangle(tri[0], tri[2], tri[1], (Color){200, 200, 255, 40});
            DrawTriangleLines(tri[0], tri[2], tri[1], (Color){200, 200, 255, 180});
        } else if (el->type == ELEM_LENS) {
            float half_h = el->size * 0.5f;
            Vector2 c = el->center;
            /* convex lens shape: two arcs approximated by lines */
            DrawLineEx((Vector2){c.x, c.y - half_h}, (Vector2){c.x, c.y + half_h}, 2.0f, (Color){200, 220, 255, 180});
            /* curved edges */
            float bulge = 12.0f;
            int segs = 20;
            for (int s = 0; s < segs; s++) {
                float t0 = (float)s / segs, t1 = (float)(s+1) / segs;
                float y0 = c.y - half_h + t0 * el->size;
                float y1 = c.y - half_h + t1 * el->size;
                float b0 = bulge * sinf(t0 * (float)M_PI);
                float b1 = bulge * sinf(t1 * (float)M_PI);
                DrawLineV((Vector2){c.x - b0, y0}, (Vector2){c.x - b1, y1}, (Color){200, 220, 255, 140});
                DrawLineV((Vector2){c.x + b0, y0}, (Vector2){c.x + b1, y1}, (Color){200, 220, 255, 140});
            }
            /* focal point markers */
            float focal = el->size * 0.8f;
            DrawCircleV((Vector2){c.x + focal, c.y}, 3, COL_ACCENT);
            DrawCircleV((Vector2){c.x - focal, c.y}, 3, COL_ACCENT);
        }
    }

    /* draw emitter */
    float cy = view.y + view.height * 0.5f;
    DrawCircleV((Vector2){view.x + 12, cy}, 6, wavelength_to_color(wavelength));
    float ang = emitter_angle * DEG2RAD;
    DrawLineEx((Vector2){view.x + 12, cy},
               (Vector2){view.x + 42, cy + sinf(ang) * 30}, 2.0f, COL_TEXT_DIM);

    /* draw photons */
    for (int i = 0; i < photon_count; i++) {
        if (!photons[i].active) continue;
        DrawCircleV(photons[i].pos, 3, photons[i].col);
    }

    EndScissorMode();
}

/* ── diffraction pattern ── */
static void draw_diffraction(Rectangle view) {
    DrawRectangleRec(view, COL_BG);
    ui_scissor_begin(view.x, view.y, view.width, view.height);

    float vw = view.width;
    float vh = view.height;
    float cx = view.x + vw * 0.5f;

    /* barrier with slit(s) */
    float barrier_x = view.x + vw * 0.25f;
    float slit_px  = slit_width * 0.3f;   /* scale to pixels */
    float sep_px   = slit_sep * 0.3f;

    DrawRectangle((int)barrier_x - 2, (int)view.y, 4, (int)vh, COL_TEXT_DIM);

    if (diff_slits == 1) {
        float sy = view.y + vh * 0.5f - slit_px * 0.5f;
        DrawRectangle((int)barrier_x - 2, (int)sy, 4, (int)slit_px, COL_BG);
    } else {
        float sy1 = view.y + vh * 0.5f - sep_px * 0.5f - slit_px * 0.5f;
        float sy2 = view.y + vh * 0.5f + sep_px * 0.5f - slit_px * 0.5f;
        DrawRectangle((int)barrier_x - 2, (int)sy1, 4, (int)slit_px, COL_BG);
        DrawRectangle((int)barrier_x - 2, (int)sy2, 4, (int)slit_px, COL_BG);
    }

    /* compute intensity pattern on right "screen" */
    float scr_x = view.x + vw * 0.70f;
    DrawLine((int)scr_x, (int)view.y, (int)scr_x, (int)(view.y + vh), COL_GRID);

    float lambda = diff_wavelength * 1e-9f;
    float a_m    = slit_width * 1e-6f;
    float d_m    = slit_sep * 1e-6f;
    Color light_col = wavelength_to_color(diff_wavelength);

    /* adaptive angular range so fringes are always visible */
    float max_angle = 3.0f * lambda / a_m;
    if (max_angle > 0.3f) max_angle = 0.3f;
    if (max_angle < 0.01f) max_angle = 0.01f;

    int steps = (int)vh;
    float avail_w = vw * 0.25f;
    for (int py = 0; py < steps; py++) {
        float t = (float)py / steps;               /* 0..1 */
        float sin_t = (t - 0.5f) * 2.0f * sinf(max_angle); /* symmetric about centre */

        /* single-slit envelope: sinc²(π a sinθ / λ) */
        float alpha_s = (float)M_PI * a_m * sin_t / lambda;
        float env = (fabsf(alpha_s) < 1e-6f) ? 1.0f : sinf(alpha_s) / alpha_s;
        float I = env * env;

        /* double-slit interference: cos²(π d sinθ / λ) */
        if (diff_slits == 2) {
            float beta = (float)M_PI * d_m * sin_t / lambda;
            float interf = cosf(beta);
            I *= interf * interf;
        }

        if (I < 0.005f) continue;
        unsigned char a_val = (unsigned char)(I * 255);
        Color c = {light_col.r, light_col.g, light_col.b, a_val};
        float screen_y = view.y + (float)py;

        /* draw intensity bars symmetrically around screen line */
        float bar_w = I * avail_w;
        DrawRectangle((int)(scr_x - bar_w * 0.5f), (int)screen_y, (int)bar_w, 1, c);
    }

    /* draw incoming wave arrows */
    Color wave_col = {light_col.r, light_col.g, light_col.b, 100};
    for (int i = 0; i < 5; i++) {
        float ay = view.y + vh * (0.3f + 0.1f * i);
        DrawLineEx((Vector2){view.x + 10, ay}, (Vector2){barrier_x - 6, ay}, 1.0f, wave_col);
        DrawTriangle(
            (Vector2){barrier_x - 6, ay},
            (Vector2){barrier_x - 14, ay - 3},
            (Vector2){barrier_x - 14, ay + 3},
            wave_col);
    }

    /* label */
    const char *mode_label = diff_slits == 1 ? "Single-Slit" : "Double-Slit";
    int lw = ui_measure_text(mode_label, FONT_SIZE_SMALL);
    DrawRectangleRounded(
        (Rectangle){cx - lw/2.0f - 8, view.y + 8, (float)(lw + 16), (float)(FONT_SIZE_SMALL + 6)},
        0.3f, 6, (Color){COL_PANEL.r, COL_PANEL.g, COL_PANEL.b, 200}
    );
    ui_draw_text(mode_label, (int)(cx - lw/2.0f), (int)(view.y + 10), FONT_SIZE_SMALL, COL_TEXT);

    EndScissorMode();
}

/* ── module callbacks ── */
static void optics_init(void) {
    optics_mode     = OPTICS_PHOTON;
    photon_running  = true;
    photon_ready    = false;
    photon_count    = 0;
    emitter_angle   = 0.0f;
    wavelength      = 550.0f;
    active_elem_type = ELEM_MIRROR;
    diff_slits      = 2;
    slit_width      = 40.0f;
    slit_sep        = 120.0f;
    diff_wavelength = 550.0f;
}

static void optics_update(Rectangle area) {
    Rectangle panel = {0}, view = {0};
    optics_layout(area, &panel, &view, NULL);
    (void)panel;
    if (optics_mode == OPTICS_PHOTON) update_photon_sim(view);
}

static void optics_draw(Rectangle area) {
    Rectangle panel = {0}, view = {0};
    bool side_by_side = true;
    optics_layout(area, &panel, &view, &side_by_side);

    DrawRectangleRec(panel, COL_PANEL);
    if (side_by_side)
        DrawLine((int)(panel.x+panel.width),(int)panel.y,
                 (int)(panel.x+panel.width),(int)(panel.y+panel.height), COL_GRID);
    else
        DrawLine((int)panel.x,(int)(panel.y+panel.height),
                 (int)(panel.x+panel.width),(int)(panel.y+panel.height), COL_GRID);

    float sx = panel.x + 8;
    float sw = panel.width - 16;
    float sy = panel.y + 8;

    ui_draw_text("Optics", (int)sx, (int)sy, FONT_SIZE_LARGE, COL_ACCENT);
    sy += 32;

    /* mode toggle */
    Rectangle toggle = { sx, sy, sw, 28 };
    Rectangle left  = { toggle.x,                    toggle.y, toggle.width/2-2, toggle.height };
    Rectangle right = { toggle.x + toggle.width/2+2, toggle.y, toggle.width/2-2, toggle.height };
    if (draw_seg_button(left,  "Photon",      optics_mode == OPTICS_PHOTON))      optics_mode = OPTICS_PHOTON;
    if (draw_seg_button(right, "Diffraction", optics_mode == OPTICS_DIFFRACTION)) optics_mode = OPTICS_DIFFRACTION;
    sy += 36;

    if (optics_mode == OPTICS_PHOTON) {
        /* element type selector */
        ui_draw_text("Element", (int)sx, (int)(sy + 2), FONT_SIZE_SMALL, COL_TEXT_DIM);
        sy += 22;
        float btn_w = (sw - 6) / 2;
        float btn_h = 26;
        for (int i = 0; i < ELEM_COUNT; i++) {
            int col = i % 2;
            float bx = sx + col * (btn_w + 6);
            Rectangle eb = {bx, sy, btn_w, btn_h};
            if (draw_seg_button(eb, elem_names[i], active_elem_type == i)) {
                if (active_elem_type != i) {
                    active_elem_type = i;
                    reset_photon_sim(view);
                }
            }
            if (col == 1) sy += btn_h + 4;
        }
        if (ELEM_COUNT % 2 == 1) sy += btn_h + 4;
        sy += 8;

        draw_param("Wavelength", &wavelength, 10, 380, 780, sx, &sy, sw, " nm");
        draw_param("Angle",      &emitter_angle, 5, -80, 80, sx, &sy, sw, "\xC2\xB0");

        Rectangle btn = { sx, sy, sw, 28 };
        if (draw_btn(btn, photon_running ? "Pause" : "Start"))
            photon_running = !photon_running;
        sy += 36;
        if (draw_btn((Rectangle){sx, sy, sw, 28}, "Reset"))
            reset_photon_sim(view);

        draw_photon_sim(view);
    } else {
        float dslits_f = (float)diff_slits;
        draw_param("Slits",      &dslits_f,      1, 1, 2, sx, &sy, sw, "");
        diff_slits = (int)dslits_f;
        draw_param("Slit Width", &slit_width,    5, 10, 200, sx, &sy, sw, " \xC2\xB5m");
        if (diff_slits == 2)
            draw_param("Slit Sep",   &slit_sep,  10, 20, 400, sx, &sy, sw, " \xC2\xB5m");
        draw_param("Wavelength", &diff_wavelength, 10, 380, 780, sx, &sy, sw, " nm");

        draw_diffraction(view);
    }
}

static void optics_cleanup(void) { /* nothing to free */ }

static Module optics_mod = {
    .name    = "Optics",
    .help_text = "Photon: Photons emit from left, interact with optical elements.\n"
                 "  Choose Mirror, Glass, Prism, or Lens.\n"
                 "  Adjust wavelength (380-780 nm) and emitter angle.\n"
                 "Diffraction: Single/double slit interference patterns.\n"
                 "  Adjust slit width, separation, and wavelength.\n"
                 "Press [H] to toggle this help.",
    .init    = optics_init,
    .update  = optics_update,
    .draw    = optics_draw,
    .cleanup = optics_cleanup,
};

Module *optics_module(void) { return &optics_mod; }
