#include "physics.h"
#include "../../ui/ui.h"
#include "../../ui/theme.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// ---- Atom model definitions ----

typedef enum {
    MODEL_DALTON,
    MODEL_THOMSON,
    MODEL_RUTHERFORD,
    MODEL_BOHR,
    MODEL_QUANTUM,
    MODEL_COUNT
} AtomModel;

static const char *model_names[] = {
    "Dalton (1803)",
    "Thomson (1897)",
    "Rutherford (1911)",
    "Bohr (1913)",
    "Quantum (1926+)",
};

static const char *model_descriptions[] = {
    // Dalton
    "John Dalton proposed that all matter\n"
    "is made of indivisible atoms — tiny,\n"
    "solid spheres that cannot be broken\n"
    "down further.\n\n"
    "Key Ideas:\n"
    "- Atoms are indivisible\n"
    "- All atoms of an element are identical\n"
    "- Atoms combine in fixed ratios\n"
    "- Chemical reactions rearrange atoms\n\n"
    "Limitation: Does not account for\n"
    "subatomic particles (electrons,\n"
    "protons, neutrons).",

    // Thomson
    "J.J. Thomson discovered the electron\n"
    "and proposed the 'plum pudding' model:\n"
    "electrons are embedded in a uniform\n"
    "sphere of positive charge.\n\n"
    "Key Ideas:\n"
    "- Atom is a sphere of positive charge\n"
    "- Electrons are scattered within it\n"
    "- Overall atom is electrically neutral\n"
    "- First subatomic particle identified\n\n"
    "Limitation: Disproved by Rutherford's\n"
    "gold foil experiment (1911).",

    // Rutherford
    "Ernest Rutherford fired alpha particles\n"
    "at gold foil. Most passed through, but\n"
    "some bounced back — proving a small,\n"
    "dense, positive nucleus exists.\n\n"
    "Key Ideas:\n"
    "- Tiny dense nucleus (positive charge)\n"
    "- Electrons orbit the nucleus\n"
    "- Atom is mostly empty space\n"
    "- Nucleus has most of the mass\n\n"
    "Limitation: Cannot explain why\n"
    "electrons don't spiral into the\n"
    "nucleus (classical EM radiation).",

    // Bohr
    "Niels Bohr proposed that electrons\n"
    "orbit the nucleus in discrete energy\n"
    "levels (shells), like planets around\n"
    "the sun.\n\n"
    "Key Ideas:\n"
    "- Fixed circular orbits (n=1,2,3...)\n"
    "- Energy is quantized: E = -13.6/n^2 eV\n"
    "- Photons emitted/absorbed when\n"
    "  electrons jump between levels\n"
    "- Explains hydrogen spectrum\n\n"
    "Limitation: Only works accurately\n"
    "for hydrogen. Fails for multi-\n"
    "electron atoms.",

    // Quantum
    "The quantum mechanical model replaces\n"
    "fixed orbits with probability clouds\n"
    "(orbitals) described by wave functions.\n\n"
    "Key Ideas:\n"
    "- Electrons exist as probability clouds\n"
    "- Orbitals: s, p, d, f shapes\n"
    "- Heisenberg uncertainty principle:\n"
    "  cannot know position & momentum\n"
    "- Schrodinger equation describes\n"
    "  electron behavior\n"
    "- Quantum numbers (n, l, ml, ms)\n\n"
    "This is the modern accepted model\n"
    "of atomic structure.",
};

// ---- Element selector ----

typedef struct {
    const char *symbol;
    const char *name;
    int protons;
    int neutrons;
    int electrons;
    int shells[7]; // electron config per shell
    int shell_count;
} Element;

static const Element elements[] = {
    {"H",  "Hydrogen",  1,  0,  1,  {1},             1},
    {"He", "Helium",    2,  2,  2,  {2},             1},
    {"Li", "Lithium",   3,  4,  3,  {2,1},           2},
    {"C",  "Carbon",    6,  6,  6,  {2,4},           2},
    {"N",  "Nitrogen",  7,  7,  7,  {2,5},           2},
    {"O",  "Oxygen",    8,  8,  8,  {2,6},           2},
    {"Na", "Sodium",    11, 12, 11, {2,8,1},         3},
    {"Fe", "Iron",      26, 30, 26, {2,8,14,2},      4},
    {"Au", "Gold",      79, 118,79, {2,8,18,32,18,1},6},
    {"U",  "Uranium",   92, 146,92, {2,8,18,32,21,9,2},7},
};
#define ELEMENT_COUNT 10

// ---- State ----

static AtomModel current_model;
static int       current_element;
static float     anim_time;

// 3D camera (orbit)
static Camera3D  cam;
static float     orbit_angle;
static float     orbit_pitch;
static float     orbit_dist;
static bool      orbiting;
static Vector2   orbit_start;
static float     orbit_angle0;
static float     orbit_pitch0;

// Sidebar scroll
static float     info_scroll;

#define SIDEBAR_W 360

static void physics_init(void) {
    current_model   = MODEL_BOHR;
    current_element = 0;
    anim_time       = 0;
    orbit_angle     = 0.6f;
    orbit_pitch     = 0.4f;
    orbit_dist      = 10.0f;
    orbiting        = false;
    info_scroll     = 0;

    cam.target     = (Vector3){0, 0, 0};
    cam.up         = (Vector3){0, 1, 0};
    cam.fovy       = 45.0f;
    cam.projection = CAMERA_PERSPECTIVE;
    cam.position   = (Vector3){8, 5, 8};
}

static void update_cam(void) {
    float cp = cosf(orbit_pitch);
    cam.position = (Vector3){
        orbit_dist * cp * sinf(orbit_angle),
        orbit_dist * sinf(orbit_pitch),
        orbit_dist * cp * cosf(orbit_angle)
    };
}

static void physics_update(Rectangle area) {
    anim_time += GetFrameTime();

    Rectangle view3d = {
        area.x + SIDEBAR_W, area.y,
        area.width - SIDEBAR_W, area.height
    };

    Vector2 mouse = GetMousePosition();
    bool in_view = CheckCollisionPointRec(mouse, view3d);

    if (in_view && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        orbiting     = true;
        orbit_start  = mouse;
        orbit_angle0 = orbit_angle;
        orbit_pitch0 = orbit_pitch;
    }
    if (orbiting) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            orbit_angle = orbit_angle0 - (mouse.x - orbit_start.x) * 0.005f;
            orbit_pitch = orbit_pitch0 + (mouse.y - orbit_start.y) * 0.005f;
            if (orbit_pitch >  1.4f) orbit_pitch =  1.4f;
            if (orbit_pitch < -1.4f) orbit_pitch = -1.4f;
        } else {
            orbiting = false;
        }
    }
    if (in_view) {
        float wheel = GetMouseWheelMove();
        orbit_dist -= wheel * 1.5f;
        if (orbit_dist < 2.0f)  orbit_dist = 2.0f;
        if (orbit_dist > 50.0f) orbit_dist = 50.0f;
    }
    if (IsKeyPressed(KEY_HOME)) {
        orbit_angle = 0.6f;
        orbit_pitch = 0.4f;
        orbit_dist  = 10.0f;
    }

    update_cam();
}

// ---- 3D Drawing helpers ----

static void draw_nucleus(const Element *el, AtomModel model) {
    if (model == MODEL_DALTON) {
        // Solid indivisible sphere
        DrawSphere((Vector3){0,0,0}, 1.5f, (Color){180, 140, 80, 255});
        return;
    }
    if (model == MODEL_THOMSON) {
        // Positive pudding sphere (translucent)
        DrawSphere((Vector3){0,0,0}, 2.5f, (Color){60, 120, 200, 60});
        // Wireframe outline
        DrawSphereWires((Vector3){0,0,0}, 2.5f, 12, 12, (Color){60, 120, 200, 100});
        return;
    }

    // Rutherford/Bohr/Quantum: small dense nucleus
    float nuc_r = 0.3f + 0.02f * (float)el->protons;
    if (nuc_r > 0.8f) nuc_r = 0.8f;

    // Draw protons (red) and neutrons (gray) as small spheres
    int total = el->protons + el->neutrons;
    if (total <= 4) {
        // Show individual nucleons
        for (int i = 0; i < total; i++) {
            float angle = (float)i / (float)total * 6.2832f;
            float r = (total == 1) ? 0.0f : 0.2f;
            Vector3 pos = {r * cosf(angle), r * sinf(angle) * 0.5f, r * sinf(angle)};
            Color c = (i < el->protons)
                ? (Color){220, 60, 60, 255}   // proton red
                : (Color){160, 160, 170, 255}; // neutron gray
            DrawSphere(pos, 0.18f, c);
        }
    } else {
        // For larger nuclei, draw a clustered sphere
        DrawSphere((Vector3){0,0,0}, nuc_r, (Color){200, 80, 80, 220});
        DrawSphereWires((Vector3){0,0,0}, nuc_r, 8, 8, (Color){160, 160, 170, 120});
    }
}

static void draw_electron_particle(Vector3 pos, float radius) {
    DrawSphere(pos, radius, (Color){60, 160, 255, 240});
    DrawSphere(pos, radius * 0.5f, (Color){180, 220, 255, 255});
}

static void draw_orbit_ring(float radius, float tilt_x, float tilt_z) {
    int segs = 64;
    for (int i = 0; i < segs; i++) {
        float a0 = (float)i / segs * 6.2832f;
        float a1 = (float)(i + 1) / segs * 6.2832f;
        Vector3 p0 = {
            radius * cosf(a0),
            radius * sinf(a0) * sinf(tilt_x),
            radius * sinf(a0) * cosf(tilt_z)
        };
        Vector3 p1 = {
            radius * cosf(a1),
            radius * sinf(a1) * sinf(tilt_x),
            radius * sinf(a1) * cosf(tilt_z)
        };
        DrawLine3D(p0, p1, (Color){60, 160, 255, 60});
    }
}

static void draw_electrons_bohr(const Element *el, float t) {
    for (int s = 0; s < el->shell_count; s++) {
        float shell_r = 1.5f + s * 1.2f;
        int n_elec = el->shells[s];

        // Draw orbit ring
        draw_orbit_ring(shell_r, 0.0f, 0.0f);

        // Draw electrons on this shell
        float speed = 1.5f / (1.0f + s * 0.3f);
        for (int e = 0; e < n_elec; e++) {
            float angle = t * speed + (float)e / n_elec * 6.2832f;
            Vector3 pos = {
                shell_r * cosf(angle),
                0.0f,
                shell_r * sinf(angle)
            };
            draw_electron_particle(pos, 0.1f);
        }
    }
}

static void draw_electrons_thomson(const Element *el, float t) {
    // Electrons embedded inside the pudding
    for (int i = 0; i < el->electrons && i < 20; i++) {
        // Use golden ratio sphere distribution
        float y = 1.0f - (2.0f * i / (float)(el->electrons - 1 > 0 ? el->electrons - 1 : 1));
        float radius_at_y = sqrtf(1.0f - y * y);
        float theta = (float)i * 2.39996f + t * 0.5f; // golden angle + animation
        float r = 1.8f; // inside the pudding
        Vector3 pos = {
            r * radius_at_y * cosf(theta),
            r * y,
            r * radius_at_y * sinf(theta)
        };
        draw_electron_particle(pos, 0.1f);
    }
}

static void draw_electrons_rutherford(const Element *el, float t) {
    // Random-ish orbits at various tilts
    int shown = el->electrons;
    if (shown > 18) shown = 18;
    for (int i = 0; i < shown; i++) {
        float shell_r = 1.5f + (i % 3) * 1.2f;
        float tilt_x = (float)i * 1.1f;
        float tilt_z = (float)i * 0.7f;
        float speed = 1.2f + (float)(i % 4) * 0.3f;
        float angle = t * speed + (float)i * 2.094f;
        Vector3 pos = {
            shell_r * cosf(angle),
            shell_r * sinf(angle) * sinf(tilt_x),
            shell_r * sinf(angle) * cosf(tilt_z)
        };
        draw_electron_particle(pos, 0.08f);
    }
}

static void draw_orbital_cloud(float t) {
    // Draw probability cloud as scattered translucent dots (s orbital approximation)
    int dots = 300;
    for (int i = 0; i < dots; i++) {
        // Pseudo-random using i and t for subtle animation
        float fi = (float)i;
        float phi   = fi * 2.39996f; // golden angle
        float costh = 1.0f - 2.0f * (fi / (float)dots);
        float sinth = sqrtf(1.0f - costh * costh);
        // Radial distribution: exponential decay (1s orbital)
        float r_raw = (fi * 7.13f + 3.7f);
        r_raw = r_raw - (int)r_raw; // fract
        float r = -logf(r_raw + 0.01f) * 0.8f;
        if (r > 4.0f) r = 4.0f;

        float wobble = sinf(t * 0.3f + fi * 0.1f) * 0.05f;
        Vector3 pos = {
            (r + wobble) * sinth * cosf(phi),
            (r + wobble) * costh,
            (r + wobble) * sinth * sinf(phi)
        };

        unsigned char alpha = (unsigned char)(180.0f * expf(-r * 0.7f));
        if (alpha < 10) continue;
        DrawSphere(pos, 0.04f, (Color){80, 160, 255, alpha});
    }
}

static void draw_atom_3d(const Element *el, AtomModel model, float t) {
    draw_nucleus(el, model);

    switch (model) {
    case MODEL_DALTON:
        // No electrons — solid sphere only
        break;
    case MODEL_THOMSON:
        draw_electrons_thomson(el, t);
        break;
    case MODEL_RUTHERFORD:
        draw_electrons_rutherford(el, t);
        break;
    case MODEL_BOHR:
        draw_electrons_bohr(el, t);
        break;
    case MODEL_QUANTUM:
        draw_orbital_cloud(t);
        break;
    default:
        break;
    }
}

// ---- Sidebar drawing ----

static void draw_model_selector(float x, float *y, float w) {
    ui_draw_text("Atom Model", (int)x + 2, (int)*y, FONT_SIZE_LARGE, COL_ACCENT);
    *y += 30;

    for (int i = 0; i < MODEL_COUNT; i++) {
        Rectangle btn = { x, *y, w, 28 };
        Vector2 mouse = GetMousePosition();
        bool hov = CheckCollisionPointRec(mouse, btn);
        bool sel = (i == (int)current_model);

        Color bg = sel ? COL_ACCENT : (hov ? (Color){50, 52, 62, 255} : COL_TAB);
        DrawRectangleRounded(btn, 0.2f, 6, bg);

        Color fg = sel ? WHITE : (hov ? COL_TEXT : COL_TEXT_DIM);
        int tw = ui_measure_text(model_names[i], FONT_SIZE_SMALL);
        ui_draw_text(model_names[i],
                     (int)(x + (w - tw) / 2), (int)(*y + (28 - FONT_SIZE_SMALL) / 2),
                     FONT_SIZE_SMALL, fg);

        if (hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            current_model = (AtomModel)i;
        }
        *y += 32;
    }
    *y += 4;
}

static void draw_element_selector(float x, float *y, float w) {
    ui_draw_text("Element", (int)x + 2, (int)*y, FONT_SIZE_DEFAULT, COL_ACCENT);
    *y += 26;

    // Button row (wrapping)
    float bx = x;
    float btn_w = 44;
    float btn_h = 30;
    float gap = 3;

    for (int i = 0; i < ELEMENT_COUNT; i++) {
        if (bx + btn_w > x + w) {
            bx = x;
            *y += btn_h + gap;
        }
        Rectangle btn = { bx, *y, btn_w, btn_h };
        Vector2 mouse = GetMousePosition();
        bool hov = CheckCollisionPointRec(mouse, btn);
        bool sel = (i == current_element);

        Color bg = sel ? PLOT_COLORS[i % PLOT_COLOR_COUNT]
                       : (hov ? (Color){50, 52, 62, 255} : COL_TAB);
        DrawRectangleRounded(btn, 0.25f, 6, bg);

        Color fg = sel ? WHITE : (hov ? COL_TEXT : COL_TEXT_DIM);
        int tw = ui_measure_text(elements[i].symbol, FONT_SIZE_SMALL);
        ui_draw_text(elements[i].symbol,
                     (int)(bx + (btn_w - tw) / 2),
                     (int)(*y + (btn_h - FONT_SIZE_SMALL) / 2),
                     FONT_SIZE_SMALL, fg);

        if (hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            current_element = i;
        }
        bx += btn_w + gap;
    }
    *y += btn_h + 8;
}

static void draw_element_info(float x, float *y, float w) {
    const Element *el = &elements[current_element];

    // Element name + symbol
    char title[64];
    snprintf(title, sizeof(title), "%s (%s)", el->name, el->symbol);
    ui_draw_text(title, (int)x + 2, (int)*y, FONT_SIZE_DEFAULT, COL_TEXT);
    *y += 24;

    // Stats
    char buf[128];
    snprintf(buf, sizeof(buf), "Protons: %d   Neutrons: %d", el->protons, el->neutrons);
    ui_draw_text(buf, (int)x + 2, (int)*y, FONT_SIZE_SMALL, COL_TEXT_DIM);
    *y += 18;

    snprintf(buf, sizeof(buf), "Electrons: %d", el->electrons);
    ui_draw_text(buf, (int)x + 2, (int)*y, FONT_SIZE_SMALL, COL_TEXT_DIM);
    *y += 18;

    // Electron configuration
    char config[128] = "Shells: ";
    for (int i = 0; i < el->shell_count; i++) {
        char tmp[16];
        snprintf(tmp, sizeof(tmp), "%s%d", i > 0 ? "-" : "", el->shells[i]);
        strcat(config, tmp);
    }
    ui_draw_text(config, (int)x + 2, (int)*y, FONT_SIZE_SMALL, COL_TEXT_DIM);
    *y += 24;

    // Separator
    DrawLine((int)x, (int)*y, (int)(x + w), (int)*y, COL_GRID);
    *y += 8;
}

// Draw multiline text manually
static void draw_multiline(const char *text, float x, float *y, float w, int font_size, Color color) {
    (void)w;
    const char *p = text;
    char line[256];
    while (*p) {
        int li = 0;
        while (*p && *p != '\n' && li < 254) {
            line[li++] = *p++;
        }
        line[li] = '\0';
        if (*p == '\n') p++;
        ui_draw_text(line, (int)x + 2, (int)*y, font_size, color);
        *y += font_size + 3;
    }
}

static void physics_draw(Rectangle area) {
    // Sidebar
    Rectangle sidebar = { area.x, area.y, SIDEBAR_W, area.height };
    DrawRectangleRec(sidebar, COL_PANEL);
    DrawLine((int)(area.x + SIDEBAR_W), (int)area.y,
             (int)(area.x + SIDEBAR_W), (int)(area.y + area.height), COL_GRID);

    float sx = area.x + 8;
    float sw = SIDEBAR_W - 16;
    float sy = area.y + 8;

    // Model selector
    draw_model_selector(sx, &sy, sw);

    // Separator
    DrawLine((int)sx, (int)sy, (int)(sx + sw), (int)sy, COL_GRID);
    sy += 8;

    // Element selector
    draw_element_selector(sx, &sy, sw);

    // Element info
    draw_element_info(sx, &sy, sw);

    // Description (scrollable)
    float desc_top = sy;
    float desc_bottom = area.y + area.height - 30;

    BeginScissorMode((int)area.x, (int)desc_top, SIDEBAR_W, (int)(desc_bottom - desc_top));

    float dy = desc_top - info_scroll;
    ui_draw_text("Description", (int)sx + 2, (int)dy, FONT_SIZE_DEFAULT, COL_ACCENT);
    dy += 24;

    draw_multiline(model_descriptions[current_model], sx, &dy, sw, FONT_SIZE_SMALL, COL_TEXT_DIM);

    EndScissorMode();

    // Scroll
    Vector2 mouse = GetMousePosition();
    if (CheckCollisionPointRec(mouse, sidebar)) {
        info_scroll -= GetMouseWheelMove() * 25.0f;
        float max_s = dy + info_scroll - desc_bottom;
        if (max_s < 0) max_s = 0;
        if (info_scroll > max_s) info_scroll = max_s;
        if (info_scroll < 0) info_scroll = 0;
    }

    // Help footer
    ui_draw_text("Drag=Orbit  Scroll=Zoom  Home=Reset", (int)sx, (int)(area.y + area.height - 20), FONT_SIZE_TINY, COL_TEXT_DIM);

    // ---- 3D view ----
    Rectangle view3d = {
        area.x + SIDEBAR_W, area.y,
        area.width - SIDEBAR_W, area.height
    };

    DrawRectangleRec(view3d, COL_BG);

    BeginScissorMode((int)view3d.x, (int)view3d.y, (int)view3d.width, (int)view3d.height);
    BeginMode3D(cam);

    draw_atom_3d(&elements[current_element], current_model, anim_time);

    EndMode3D();
    EndScissorMode();

    // Model name overlay
    {
        const char *mn = model_names[current_model];
        int tw = ui_measure_text(mn, FONT_SIZE_DEFAULT);
        float ox = view3d.x + (view3d.width - tw) / 2;
        float oy = view3d.y + 10;
        DrawRectangleRounded(
            (Rectangle){ox - 8, oy - 4, (float)(tw + 16), (float)(FONT_SIZE_DEFAULT + 8)},
            0.3f, 6, (Color){COL_PANEL.r, COL_PANEL.g, COL_PANEL.b, 200}
        );
        ui_draw_text(mn, (int)ox, (int)oy, FONT_SIZE_DEFAULT, COL_ACCENT);
    }

    // Element overlay
    {
        const Element *el = &elements[current_element];
        char elabel[64];
        snprintf(elabel, sizeof(elabel), "%s - %s  (Z=%d)", el->symbol, el->name, el->protons);
        int tw = ui_measure_text(elabel, FONT_SIZE_SMALL);
        float ox = view3d.x + (view3d.width - tw) / 2;
        float oy = view3d.y + 38;
        DrawRectangleRounded(
            (Rectangle){ox - 6, oy - 2, (float)(tw + 12), (float)(FONT_SIZE_SMALL + 4)},
            0.3f, 6, (Color){COL_PANEL.r, COL_PANEL.g, COL_PANEL.b, 180}
        );
        ui_draw_text(elabel, (int)ox, (int)oy, FONT_SIZE_SMALL, COL_TEXT);
    }

    // Legend (bottom of 3D view)
    {
        float lx = view3d.x + 12;
        float ly = view3d.y + view3d.height - 50;
        DrawCircle((int)lx + 6, (int)ly + 6, 5, (Color){220, 60, 60, 255});
        ui_draw_text("Proton", (int)lx + 16, (int)ly, FONT_SIZE_TINY, COL_TEXT_DIM);
        ly += 16;
        DrawCircle((int)lx + 6, (int)ly + 6, 5, (Color){160, 160, 170, 255});
        ui_draw_text("Neutron", (int)lx + 16, (int)ly, FONT_SIZE_TINY, COL_TEXT_DIM);
        ly += 16;
        DrawCircle((int)lx + 6, (int)ly + 6, 5, (Color){60, 160, 255, 255});
        ui_draw_text("Electron", (int)lx + 16, (int)ly, FONT_SIZE_TINY, COL_TEXT_DIM);
    }
}

static void physics_cleanup(void) {
    // Nothing to free
}

static Module physics_mod = {
    .name    = "Physics",
    .init    = physics_init,
    .update  = physics_update,
    .draw    = physics_draw,
    .cleanup = physics_cleanup,
};

Module *physics_module(void) {
    return &physics_mod;
}
