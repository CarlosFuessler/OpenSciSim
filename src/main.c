#include "raylib.h"
#include "ui/ui.h"
#include "ui/theme.h"
#include "modules/cas/cas.h"
#include "modules/mathsim/mathsim.h"
#include "modules/calc/calc.h"
#include "modules/physics/physics.h"
#include "modules/physics/mechanics.h"
#include "modules/chemistry/chemistry.h"
#include "modules/chemistry/chemsim.h"
#include <stddef.h>

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

Font g_font;
static AppUI ui;

static void game_frame(void) {
    ui_update(&ui);
    BeginDrawing();
    ui_draw(&ui);
    EndDrawing();
}

int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(WINDOW_W, WINDOW_H, "OpenSciSim - Interactive Science Simulator");
    SetTargetFPS(TARGET_FPS);

    // Load custom font at high resolution for crisp rendering at all sizes
    g_font = LoadFontEx(FONT_PATH, FONT_LOAD_SIZE, NULL, 0);
    SetTextureFilter(g_font.texture, TEXTURE_FILTER_BILINEAR);

    ui_init(&ui);

    // Create topics
    int math = ui_add_topic(&ui, "Mathematics", "CAS, Plotter & Calculator", (Color){66, 165, 245, 255});
    int phys = ui_add_topic(&ui, "Physics",     "Atom Models & Simulations", (Color){239, 83, 80, 255});
    int chem = ui_add_topic(&ui, "Chemistry",   "Periodic Table & Molecules", (Color){102, 187, 106, 255});

    // Register modules into topics
    ui_register_module(&ui, math, cas_module());
    ui_register_module(&ui, math, calc_module());
    ui_register_module(&ui, math, mathsim_module());
    ui_register_module(&ui, phys, physics_module());
    ui_register_module(&ui, phys, mechanics_module());
    ui_register_module(&ui, chem, chemistry_module());
    ui_register_module(&ui, chem, chemsim_module());

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(game_frame, TARGET_FPS, 1);
#else
    while (!WindowShouldClose()) {
        game_frame();
    }
#endif

    // Cleanup all modules
    for (int t = 0; t < ui.topic_count; t++) {
        for (int m = 0; m < ui.topics[t].module_count; m++) {
            if (ui.topics[t].modules[m]->cleanup)
                ui.topics[t].modules[m]->cleanup();
        }
    }

    UnloadFont(g_font);
    CloseWindow();
    return 0;
}
