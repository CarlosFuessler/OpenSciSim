#include "plotter.h"
#include "eval.h"
#include "../../ui/ui.h"
#include "../../ui/theme.h"
#include <math.h>
#include <stdio.h>

void plotter_init(PlotState *ps) {
    ps->center_x  = 0.0;
    ps->center_y  = 0.0;
    ps->scale     = 80.0;
    ps->func_count = 0;
    ps->dragging   = false;
}

static Vector2 math_to_screen(PlotState *ps, Rectangle area, double mx, double my) {
    Vector2 v;
    v.x = (float)(area.x + area.width  / 2.0 + (mx - ps->center_x) * ps->scale);
    v.y = (float)(area.y + area.height / 2.0 - (my - ps->center_y) * ps->scale);
    return v;
}

void plotter_update(PlotState *ps, Rectangle area) {
    Vector2 mouse = ui_mouse();
    bool in_area = CheckCollisionPointRec(mouse, area);

    // Zoom with scroll wheel
    if (in_area) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            double mx_before = ps->center_x + (mouse.x - area.x - area.width / 2.0) / ps->scale;
            double my_before = ps->center_y - (mouse.y - area.y - area.height / 2.0) / ps->scale;

            double factor = (wheel > 0) ? 1.15 : 1.0 / 1.15;
            ps->scale *= factor;
            if (ps->scale < 2.0)    ps->scale = 2.0;
            if (ps->scale > 10000.0) ps->scale = 10000.0;

            double mx_after = ps->center_x + (mouse.x - area.x - area.width / 2.0) / ps->scale;
            double my_after = ps->center_y - (mouse.y - area.y - area.height / 2.0) / ps->scale;

            ps->center_x += mx_before - mx_after;
            ps->center_y += my_before - my_after;
        }
    }

    // Pan with mouse drag
    if (in_area && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        ps->dragging   = true;
        ps->drag_start = mouse;
        ps->drag_cx    = ps->center_x;
        ps->drag_cy    = ps->center_y;
    }
    if (ps->dragging) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            double dx = (mouse.x - ps->drag_start.x) / ps->scale;
            double dy = (mouse.y - ps->drag_start.y) / ps->scale;
            ps->center_x = ps->drag_cx - dx;
            ps->center_y = ps->drag_cy + dy;
        } else {
            ps->dragging = false;
        }
    }

    // Reset view with Home key
    if (IsKeyPressed(KEY_HOME)) {
        ps->center_x = 0.0;
        ps->center_y = 0.0;
        ps->scale    = 80.0;
    }
}

static void draw_grid(PlotState *ps, Rectangle area) {
    double raw_step = 60.0 / ps->scale;
    double mag = pow(10.0, floor(log10(raw_step)));
    double norm = raw_step / mag;
    double step;
    if (norm < 2.0)      step = 2.0 * mag;
    else if (norm < 5.0) step = 5.0 * mag;
    else                 step = 10.0 * mag;

    double x_min = ps->center_x - (area.width / 2.0) / ps->scale;
    double x_max = ps->center_x + (area.width / 2.0) / ps->scale;
    double y_min = ps->center_y - (area.height / 2.0) / ps->scale;
    double y_max = ps->center_y + (area.height / 2.0) / ps->scale;

    ui_scissor_begin((int)area.x, (int)area.y, (int)area.width, (int)area.height);

    // Sub-grid (lighter)
    double sub_step = step / 5.0;
    Color sub_col = (Color){42, 44, 50, 255};
    double sx_start = floor(x_min / sub_step) * sub_step;
    for (double gx = sx_start; gx <= x_max; gx += sub_step) {
        Vector2 top = math_to_screen(ps, area, gx, y_max);
        Vector2 bot = math_to_screen(ps, area, gx, y_min);
        DrawLineV(top, bot, sub_col);
    }
    double sy_start = floor(y_min / sub_step) * sub_step;
    for (double gy = sy_start; gy <= y_max; gy += sub_step) {
        Vector2 left  = math_to_screen(ps, area, x_min, gy);
        Vector2 right = math_to_screen(ps, area, x_max, gy);
        DrawLineV(left, right, sub_col);
    }

    // Major grid lines
    double x_start = floor(x_min / step) * step;
    for (double gx = x_start; gx <= x_max; gx += step) {
        Vector2 top = math_to_screen(ps, area, gx, y_max);
        Vector2 bot = math_to_screen(ps, area, gx, y_min);
        DrawLineV(top, bot, COL_GRID);

        char label[32];
        snprintf(label, sizeof(label), "%.4g", gx);
        Vector2 axis_pos = math_to_screen(ps, area, gx, 0);
        // Clamp label to plot area
        float ly = axis_pos.y + 4;
        if (ly < area.y + 2) ly = area.y + 2;
        if (ly > area.y + area.height - 16) ly = area.y + area.height - 16;
        ui_draw_text(label, (int)axis_pos.x + 4, (int)ly, FONT_SIZE_TINY, COL_TEXT_DIM);
    }

    double y_start = floor(y_min / step) * step;
    for (double gy = y_start; gy <= y_max; gy += step) {
        Vector2 left  = math_to_screen(ps, area, x_min, gy);
        Vector2 right = math_to_screen(ps, area, x_max, gy);
        DrawLineV(left, right, COL_GRID);

        if (fabs(gy) > step * 0.01) {
            char label[32];
            snprintf(label, sizeof(label), "%.4g", gy);
            Vector2 axis_pos = math_to_screen(ps, area, 0, gy);
            float lx = axis_pos.x + 4;
            if (lx < area.x + 2) lx = area.x + 2;
            ui_draw_text(label, (int)lx, (int)axis_pos.y - 14, FONT_SIZE_TINY, COL_TEXT_DIM);
        }
    }

    // Axes (thicker)
    Vector2 ax_left  = math_to_screen(ps, area, x_min, 0);
    Vector2 ax_right = math_to_screen(ps, area, x_max, 0);
    DrawLineEx(ax_left, ax_right, 2.0f, COL_AXIS);

    Vector2 ax_top = math_to_screen(ps, area, 0, y_max);
    Vector2 ax_bot = math_to_screen(ps, area, 0, y_min);
    DrawLineEx(ax_top, ax_bot, 2.0f, COL_AXIS);

    // Origin marker
    Vector2 origin = math_to_screen(ps, area, 0, 0);
    DrawCircleV(origin, 3.0f, COL_AXIS);

    EndScissorMode();
}

void plotter_draw(PlotState *ps, Rectangle area, Arena *arena) {
    (void)arena;

    DrawRectangleRec(area, COL_BG);
    draw_grid(ps, area);

    ui_scissor_begin((int)area.x, (int)area.y, (int)area.width, (int)area.height);

    double x_min = ps->center_x - (area.width / 2.0) / ps->scale;

    // Draw each function
    for (int fi = 0; fi < ps->func_count; fi++) {
        if (!ps->funcs[fi].visible || !ps->funcs[fi].valid || !ps->funcs[fi].ast) continue;

        Color col = PLOT_COLORS[ps->funcs[fi].color_idx % PLOT_COLOR_COUNT];
        int steps = (int)area.width;

        Vector2 prev = {0};
        bool prev_valid = false;
        bool label_placed = false;
        // Place label at ~20% from left of plot
        int label_target_x = (int)(area.width * 0.2f);

        for (int i = 0; i <= steps; i++) {
            double mx = x_min + (double)i / ps->scale;
            double my = eval_ast(ps->funcs[fi].ast, mx);

            if (isnan(my) || isinf(my)) {
                prev_valid = false;
                continue;
            }

            Vector2 pt = math_to_screen(ps, area, mx, my);

            if (prev_valid) {
                float dy = fabsf(pt.y - prev.y);
                if (dy < area.height * 2.0f) {
                    DrawLineEx(prev, pt, 2.5f, col);
                }
            }

            // Draw label on curve
            if (!label_placed && i >= label_target_x &&
                pt.y > area.y + 20 && pt.y < area.y + area.height - 20) {
                // Background pill behind label
                const char *lbl = ps->funcs[fi].name;
                int lw = ui_measure_text(lbl, FONT_SIZE_TINY);
                DrawRectangleRounded(
                    (Rectangle){pt.x + 6, pt.y - 18, (float)(lw + 10), 20},
                    0.4f, 6, (Color){col.r, col.g, col.b, 180}
                );
                ui_draw_text(lbl, (int)pt.x + 11, (int)pt.y - 17, FONT_SIZE_TINY, WHITE);
                label_placed = true;
            }

            prev = pt;
            prev_valid = true;
        }
    }

    // Crosshair + coordinate display + function values at cursor
    Vector2 mouse = ui_mouse();
    if (CheckCollisionPointRec(mouse, area)) {
        double mx = ps->center_x + (mouse.x - area.x - area.width / 2.0) / ps->scale;
        double my = ps->center_y - (mouse.y - area.y - area.height / 2.0) / ps->scale;

        // Subtle crosshair lines
        DrawLineV((Vector2){mouse.x, area.y}, (Vector2){mouse.x, area.y + area.height},
                  (Color){70, 72, 85, 100});
        DrawLineV((Vector2){area.x, mouse.y}, (Vector2){area.x + area.width, mouse.y},
                  (Color){70, 72, 85, 100});

        // Coordinate tooltip
        char coords[64];
        snprintf(coords, sizeof(coords), "(%.3f, %.3f)", mx, my);
        int cw = ui_measure_text(coords, FONT_SIZE_TINY);
        // Background for readability
        DrawRectangleRounded(
            (Rectangle){mouse.x + 14, mouse.y - 22, (float)(cw + 10), 20},
            0.3f, 6, (Color){COL_PANEL.r, COL_PANEL.g, COL_PANEL.b, 220}
        );
        ui_draw_text(coords, (int)mouse.x + 19, (int)mouse.y - 21, FONT_SIZE_TINY, COL_TEXT);

        // Show function values at cursor x
        float info_y = mouse.y + 8;
        for (int fi = 0; fi < ps->func_count; fi++) {
            if (!ps->funcs[fi].visible || !ps->funcs[fi].valid || !ps->funcs[fi].ast) continue;
            double fy = eval_ast(ps->funcs[fi].ast, mx);
            if (isnan(fy) || isinf(fy)) continue;

            // Draw dot on curve
            Vector2 dot_pos = math_to_screen(ps, area, mx, fy);
            Color col = PLOT_COLORS[ps->funcs[fi].color_idx % PLOT_COLOR_COUNT];
            DrawCircleV(dot_pos, 4.0f, col);
            DrawCircleV(dot_pos, 2.0f, WHITE);

            // Value tooltip near cursor
            char val[80];
            snprintf(val, sizeof(val), "%s = %.4g", ps->funcs[fi].name, fy);
            int vw = ui_measure_text(val, FONT_SIZE_TINY);
            DrawRectangleRounded(
                (Rectangle){mouse.x + 14, info_y, (float)(vw + 10), 18},
                0.3f, 6, (Color){col.r, col.g, col.b, 160}
            );
            ui_draw_text(val, (int)mouse.x + 19, (int)info_y + 1, FONT_SIZE_TINY, WHITE);
            info_y += 22;
        }
    }

    EndScissorMode();
}
