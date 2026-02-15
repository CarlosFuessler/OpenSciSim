#include "raylib.h"
#include "ui/ui.h"
#include "ui/theme.h"
#include "modules/cas/cas.h"
#include "modules/calc/calc.h"
#include <stddef.h>

Font g_font;

int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(WINDOW_W, WINDOW_H, "OpenSciSim - Interactive Science Simulator");
    SetTargetFPS(TARGET_FPS);

    // Load custom font with extended character set
    g_font = LoadFontEx(FONT_PATH, 72, NULL, 0);
    SetTextureFilter(g_font.texture, TEXTURE_FILTER_BILINEAR);

    AppUI ui;
    ui_init(&ui);
    ui_register_module(&ui, cas_module());
    ui_register_module(&ui, calc_module());

    while (!WindowShouldClose()) {
        ui_update(&ui);

        BeginDrawing();
        ui_draw(&ui);
        EndDrawing();
    }

    // Cleanup all modules
    for (int i = 0; i < ui.module_count; i++) {
        if (ui.modules[i]->cleanup) ui.modules[i]->cleanup();
    }

    UnloadFont(g_font);
    CloseWindow();
    return 0;
}
