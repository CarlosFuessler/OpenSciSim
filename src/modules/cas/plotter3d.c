#include "plotter3d.h"
#include "eval.h"
#include "../../ui/ui.h"
#include "../../ui/theme.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>

#define GRID_LINES 20
#define SURF_RES   60

void plotter3d_init(Plot3DState *ps) {
    ps->orbit_angle = 0.6f;
    ps->orbit_pitch = 0.5f;
    ps->orbit_dist  = 12.0f;
    ps->orbiting    = false;
    ps->surf_count  = 0;
    ps->vec_count   = 0;
    ps->range       = 5.0f;

    ps->camera.target   = (Vector3){0, 0, 0};
    ps->camera.up       = (Vector3){0, 1, 0};
    ps->camera.fovy     = 45.0f;
    ps->camera.projection = CAMERA_PERSPECTIVE;

    // Position will be set from orbit params in update
    ps->camera.position = (Vector3){8, 6, 8};
}

static void update_camera_from_orbit(Plot3DState *ps) {
    float cp = cosf(ps->orbit_pitch);
    ps->camera.position = (Vector3){
        ps->orbit_dist * cp * sinf(ps->orbit_angle),
        ps->orbit_dist * sinf(ps->orbit_pitch),
        ps->orbit_dist * cp * cosf(ps->orbit_angle)
    };
}

void plotter3d_update(Plot3DState *ps, Rectangle area) {
    Vector2 mouse = GetMousePosition();
    bool in_area = CheckCollisionPointRec(mouse, area);

    // Orbit with left-drag
    if (in_area && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        ps->orbiting    = true;
        ps->orbit_start = mouse;
        ps->orbit_angle0 = ps->orbit_angle;
        ps->orbit_pitch0 = ps->orbit_pitch;
    }
    if (ps->orbiting) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            float dx = mouse.x - ps->orbit_start.x;
            float dy = mouse.y - ps->orbit_start.y;
            ps->orbit_angle = ps->orbit_angle0 - dx * 0.005f;
            ps->orbit_pitch = ps->orbit_pitch0 + dy * 0.005f;
            if (ps->orbit_pitch >  1.4f) ps->orbit_pitch =  1.4f;
            if (ps->orbit_pitch < -1.4f) ps->orbit_pitch = -1.4f;
        } else {
            ps->orbiting = false;
        }
    }

    // Zoom with scroll
    if (in_area) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            ps->orbit_dist -= wheel * 1.0f;
            if (ps->orbit_dist < 3.0f)  ps->orbit_dist = 3.0f;
            if (ps->orbit_dist > 40.0f) ps->orbit_dist = 40.0f;
        }
    }

    // Reset with Home
    if (IsKeyPressed(KEY_HOME)) {
        ps->orbit_angle = 0.6f;
        ps->orbit_pitch = 0.5f;
        ps->orbit_dist  = 12.0f;
        ps->range       = 5.0f;
    }

    update_camera_from_orbit(ps);
}

static void draw_axes(float range) {
    float len = range * 1.2f;

    // X axis - red
    DrawLine3D((Vector3){-len, 0, 0}, (Vector3){len, 0, 0}, (Color){200, 60, 60, 255});
    DrawCylinderEx((Vector3){len, 0, 0}, (Vector3){len + 0.3f, 0, 0}, 0.08f, 0.0f, 8, (Color){200, 60, 60, 255});

    // Y axis - green (up)
    DrawLine3D((Vector3){0, -len, 0}, (Vector3){0, len, 0}, (Color){60, 200, 60, 255});
    DrawCylinderEx((Vector3){0, len, 0}, (Vector3){0, len + 0.3f, 0}, 0.08f, 0.0f, 8, (Color){60, 200, 60, 255});

    // Z axis - blue
    DrawLine3D((Vector3){0, 0, -len}, (Vector3){0, 0, len}, (Color){60, 60, 200, 255});
    DrawCylinderEx((Vector3){0, 0, len}, (Vector3){0, 0, len + 0.3f}, 0.08f, 0.0f, 8, (Color){60, 60, 200, 255});
}

static void draw_grid_3d(float range) {
    float step = range / (GRID_LINES / 2);
    Color grid_col = (Color){50, 52, 60, 120};

    for (int i = -GRID_LINES / 2; i <= GRID_LINES / 2; i++) {
        float pos = i * step;
        // Lines along X (on XZ plane)
        DrawLine3D((Vector3){-range, 0, pos}, (Vector3){range, 0, pos}, grid_col);
        // Lines along Z
        DrawLine3D((Vector3){pos, 0, -range}, (Vector3){pos, 0, range}, grid_col);
    }
}

static void draw_vector(VecEntry *v) {
    Color col = PLOT_COLORS[v->color_idx % PLOT_COLOR_COUNT];
    Vector3 origin = {0, 0, 0};
    Vector3 tip = {v->x, v->y, v->z};

    // Shaft
    DrawLine3D(origin, tip, col);

    // Arrowhead (small cone)
    float len = sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
    if (len > 0.01f) {
        float head_len = fminf(0.3f, len * 0.15f);
        float t = (len - head_len) / len;
        Vector3 base = {v->x * t, v->y * t, v->z * t};
        DrawCylinderEx(base, tip, 0.06f, 0.0f, 8, col);
    }

    // Draw a small sphere at the tip
    DrawSphere(tip, 0.06f, col);
}

static void draw_surface(FuncSlot *slot, float range, Arena *arena) {
    (void)arena;
    if (!slot->ast || !slot->valid || !slot->visible) return;

    Color col = PLOT_COLORS[slot->color_idx % PLOT_COLOR_COUNT];
    Color col_t = (Color){col.r, col.g, col.b, 160};

    float step = (range * 2.0f) / SURF_RES;

    for (int ix = 0; ix < SURF_RES; ix++) {
        for (int iz = 0; iz < SURF_RES; iz++) {
            float x0 = -range + ix * step;
            float z0 = -range + iz * step;
            float x1 = x0 + step;
            float z1 = z0 + step;

            // Evaluate z = f(x, y) at 4 corners
            // In our coordinate system: x→x, z→y (user's y input), result→Y (up)
            double y00 = eval_ast_xy(slot->ast, x0, z0);
            double y10 = eval_ast_xy(slot->ast, x1, z0);
            double y01 = eval_ast_xy(slot->ast, x0, z1);
            double y11 = eval_ast_xy(slot->ast, x1, z1);

            // Skip if any value is invalid or too large
            if (isnan(y00) || isnan(y10) || isnan(y01) || isnan(y11)) continue;
            if (isinf(y00) || isinf(y10) || isinf(y01) || isinf(y11)) continue;
            float clamp = range * 2.0f;
            if (fabsf((float)y00) > clamp || fabsf((float)y10) > clamp ||
                fabsf((float)y01) > clamp || fabsf((float)y11) > clamp) continue;

            // Triangle 1
            DrawTriangle3D(
                (Vector3){x0, (float)y00, z0},
                (Vector3){x1, (float)y10, z0},
                (Vector3){x0, (float)y01, z1},
                col_t
            );
            // Triangle 2
            DrawTriangle3D(
                (Vector3){x1, (float)y10, z0},
                (Vector3){x1, (float)y11, z1},
                (Vector3){x0, (float)y01, z1},
                col_t
            );
            // Back faces (reverse winding)
            DrawTriangle3D(
                (Vector3){x0, (float)y01, z1},
                (Vector3){x1, (float)y10, z0},
                (Vector3){x0, (float)y00, z0},
                col_t
            );
            DrawTriangle3D(
                (Vector3){x0, (float)y01, z1},
                (Vector3){x1, (float)y11, z1},
                (Vector3){x1, (float)y10, z0},
                col_t
            );

            // Wireframe on top
            Color wire = (Color){col.r, col.g, col.b, 80};
            DrawLine3D((Vector3){x0, (float)y00, z0}, (Vector3){x1, (float)y10, z0}, wire);
            DrawLine3D((Vector3){x0, (float)y00, z0}, (Vector3){x0, (float)y01, z1}, wire);
        }
    }
}

void plotter3d_draw(Plot3DState *ps, Rectangle area, Arena *arena) {
    DrawRectangleRec(area, COL_BG);

    BeginScissorMode((int)area.x, (int)area.y, (int)area.width, (int)area.height);
    BeginMode3D(ps->camera);

    draw_grid_3d(ps->range);
    draw_axes(ps->range);

    // Draw surfaces
    for (int i = 0; i < ps->surf_count; i++) {
        draw_surface(&ps->surfs[i], ps->range, arena);
    }

    // Draw vectors
    for (int i = 0; i < ps->vec_count; i++) {
        if (ps->vecs[i].visible)
            draw_vector(&ps->vecs[i]);
    }

    EndMode3D();
    EndScissorMode();

    // Axis labels (2D overlay)
    // Project axis endpoints to screen to place labels
    Vector3 ax_tips[3] = {
        {ps->range * 1.3f, 0, 0},
        {0, ps->range * 1.3f, 0},
        {0, 0, ps->range * 1.3f}
    };
    const char *ax_names[3] = {"X", "Y", "Z"};
    Color ax_cols[3] = {{200,60,60,255}, {60,200,60,255}, {60,60,200,255}};

    for (int i = 0; i < 3; i++) {
        Vector2 sp = GetWorldToScreen(ax_tips[i], ps->camera);
        if (sp.x > area.x && sp.x < area.x + area.width &&
            sp.y > area.y && sp.y < area.y + area.height) {
            ui_draw_text(ax_names[i], (int)sp.x + 4, (int)sp.y - 8, FONT_SIZE_DEFAULT, ax_cols[i]);
        }
    }

    // Help text at bottom-right
    float hx = area.x + area.width - 260;
    float hy = area.y + area.height - 20;
    ui_draw_text("Drag=Orbit  Scroll=Zoom  Home=Reset", (int)hx, (int)hy, 11, COL_TEXT_DIM);
}
