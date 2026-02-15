#include "chemistry.h"
#include "../../ui/ui.h"
#include "../../ui/theme.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// ---- Periodic Table Data (first 36 elements) ----

typedef enum {
    CAT_NONMETAL,
    CAT_NOBLE_GAS,
    CAT_ALKALI,
    CAT_ALKALINE,
    CAT_METALLOID,
    CAT_HALOGEN,
    CAT_TRANSITION,
    CAT_POST_TRANS,
    CAT_COUNT
} ElemCategory;

static const Color cat_colors[] = {
    {102, 187, 106, 255},  // nonmetal - green
    {171,  71, 188, 255},  // noble gas - purple
    {239,  83,  80, 255},  // alkali - red
    {255, 167,  38, 255},  // alkaline - orange
    { 38, 198, 218, 255},  // metalloid - teal
    {255, 202,  40, 255},  // halogen - yellow
    { 66, 165, 245, 255},  // transition - blue
    {120, 144, 156, 255},  // post-transition - gray-blue
};

static const char *cat_names[] = {
    "Nonmetal", "Noble Gas", "Alkali Metal", "Alkaline Earth",
    "Metalloid", "Halogen", "Transition Metal", "Post-Trans. Metal"
};

typedef struct {
    int         Z;       // atomic number
    const char *symbol;
    const char *name;
    float       mass;
    int         group;   // 1-18
    int         period;  // 1-7
    ElemCategory cat;
    const char *electron_config;
    float       electronegativity; // Pauling scale, 0 if N/A
    const char *description;
} PTElement;

static const PTElement periodic_table[] = {
    {1,  "H",  "Hydrogen",   1.008f,  1, 1, CAT_NONMETAL,   "1s1",          2.20f,
     "Lightest element. Most abundant\nin the universe. Highly flammable\ndiatomic gas (H2)."},
    {2,  "He", "Helium",     4.003f,  18,1, CAT_NOBLE_GAS,  "1s2",          0.00f,
     "Second lightest element. Inert\nnoble gas. Used in balloons,\ncryogenics and welding."},
    {3,  "Li", "Lithium",    6.941f,  1, 2, CAT_ALKALI,     "[He] 2s1",     0.98f,
     "Lightest metal. Highly reactive.\nUsed in batteries, ceramics,\nand psychiatric medication."},
    {4,  "Be", "Beryllium",  9.012f,  2, 2, CAT_ALKALINE,   "[He] 2s2",     1.57f,
     "Lightweight, strong metal.\nToxic dust. Used in aerospace\nand X-ray windows."},
    {5,  "B",  "Boron",      10.81f,  13,2, CAT_METALLOID,  "[He] 2s2 2p1", 2.04f,
     "Metalloid used in borosilicate\nglass, detergents, and as a\nneutron absorber."},
    {6,  "C",  "Carbon",     12.01f,  14,2, CAT_NONMETAL,   "[He] 2s2 2p2", 2.55f,
     "Basis of organic chemistry.\nForms diamond, graphite, and\nfullerenes. Essential for life."},
    {7,  "N",  "Nitrogen",   14.01f,  15,2, CAT_NONMETAL,   "[He] 2s2 2p3", 3.04f,
     "78% of Earth's atmosphere.\nEssential for amino acids &\nDNA. Used in fertilizers."},
    {8,  "O",  "Oxygen",     16.00f,  16,2, CAT_NONMETAL,   "[He] 2s2 2p4", 3.44f,
     "21% of atmosphere. Required\nfor combustion and respiration.\nForms ozone (O3) layer."},
    {9,  "F",  "Fluorine",   19.00f,  17,2, CAT_HALOGEN,    "[He] 2s2 2p5", 3.98f,
     "Most electronegative element.\nHighly reactive pale yellow gas.\nUsed in toothpaste (fluoride)."},
    {10, "Ne", "Neon",       20.18f,  18,2, CAT_NOBLE_GAS,  "[He] 2s2 2p6", 0.00f,
     "Noble gas producing orange-red\nglow in discharge tubes. Used\nin neon signs and lasers."},
    {11, "Na", "Sodium",     22.99f,  1, 3, CAT_ALKALI,     "[Ne] 3s1",     0.93f,
     "Soft, silvery alkali metal.\nReacts violently with water.\nEssential in table salt (NaCl)."},
    {12, "Mg", "Magnesium",  24.31f,  2, 3, CAT_ALKALINE,   "[Ne] 3s2",     1.31f,
     "Light structural metal. Burns\nwith bright white flame. Essential\nmineral for the human body."},
    {13, "Al", "Aluminium",  26.98f,  13,3, CAT_POST_TRANS,  "[Ne] 3s2 3p1", 1.61f,
     "Most abundant metal in Earth's\ncrust. Lightweight, recyclable.\nUsed in cans, foil, aircraft."},
    {14, "Si", "Silicon",    28.09f,  14,3, CAT_METALLOID,  "[Ne] 3s2 3p2", 1.90f,
     "Semiconductor. Second most\nabundant element in Earth's\ncrust. Basis of computer chips."},
    {15, "P",  "Phosphorus", 30.97f,  15,3, CAT_NONMETAL,   "[Ne] 3s2 3p3", 2.19f,
     "Essential for life (DNA, ATP).\nWhite phosphorus glows in dark.\nUsed in fertilizers, matches."},
    {16, "S",  "Sulfur",     32.07f,  16,3, CAT_NONMETAL,   "[Ne] 3s2 3p4", 2.58f,
     "Yellow nonmetal with distinctive\nsmell. Used in gunpowder,\nsulfuric acid, vulcanization."},
    {17, "Cl", "Chlorine",   35.45f,  17,3, CAT_HALOGEN,    "[Ne] 3s2 3p5", 3.16f,
     "Greenish toxic gas. Used in\nwater purification, PVC,\nbleach, and disinfectants."},
    {18, "Ar", "Argon",      39.95f,  18,3, CAT_NOBLE_GAS,  "[Ne] 3s2 3p6", 0.00f,
     "Third most abundant gas in\natmosphere (~0.93%). Used in\nwelding and light bulbs."},
    {19, "K",  "Potassium",  39.10f,  1, 4, CAT_ALKALI,     "[Ar] 4s1",     0.82f,
     "Soft, reactive alkali metal.\nEssential nutrient. Reacts\nvigorously with water."},
    {20, "Ca", "Calcium",    40.08f,  2, 4, CAT_ALKALINE,   "[Ar] 4s2",     1.00f,
     "Essential for bones and teeth.\nFifth most abundant element\nin Earth's crust."},
    {21, "Sc", "Scandium",   44.96f,  3, 4, CAT_TRANSITION, "[Ar] 3d1 4s2", 1.36f,
     "Light transition metal. Used\nin aerospace alloys. Makes\naluminium alloys stronger."},
    {22, "Ti", "Titanium",   47.87f,  4, 4, CAT_TRANSITION, "[Ar] 3d2 4s2", 1.54f,
     "Strong, lightweight, corrosion-\nresistant. Used in aerospace,\nmedical implants, and paint."},
    {23, "V",  "Vanadium",   50.94f,  5, 4, CAT_TRANSITION, "[Ar] 3d3 4s2", 1.63f,
     "Hard silvery metal. Used in\nsteel alloys and vanadium\nredox flow batteries."},
    {24, "Cr", "Chromium",   52.00f,  6, 4, CAT_TRANSITION, "[Ar] 3d5 4s1", 1.66f,
     "Hard, shiny metal. Used for\nchrome plating, stainless\nsteel, and pigments."},
    {25, "Mn", "Manganese",  54.94f,  7, 4, CAT_TRANSITION, "[Ar] 3d5 4s2", 1.55f,
     "Essential in steel production.\nImportant for enzymes in\nbiological systems."},
    {26, "Fe", "Iron",       55.85f,  8, 4, CAT_TRANSITION, "[Ar] 3d6 4s2", 1.83f,
     "Most used metal. Core of Earth\nis iron. Essential for hemoglobin.\nForms steel with carbon."},
    {27, "Co", "Cobalt",     58.93f,  9, 4, CAT_TRANSITION, "[Ar] 3d7 4s2", 1.88f,
     "Blue pigment since antiquity.\nUsed in batteries (Li-ion),\nmagnets, and vitamin B12."},
    {28, "Ni", "Nickel",     58.69f,  10,4, CAT_TRANSITION, "[Ar] 3d8 4s2", 1.91f,
     "Corrosion-resistant. Used in\nstainless steel, coins,\nrechargeable batteries."},
    {29, "Cu", "Copper",     63.55f,  11,4, CAT_TRANSITION, "[Ar] 3d10 4s1",1.90f,
     "Excellent conductor. Used since\nantiquity. Electrical wiring,\nplumbing, and alloys."},
    {30, "Zn", "Zinc",       65.38f,  12,4, CAT_TRANSITION, "[Ar] 3d10 4s2",1.65f,
     "Used for galvanizing steel.\nEssential trace element. Used\nin brass alloy with copper."},
    {31, "Ga", "Gallium",    69.72f,  13,4, CAT_POST_TRANS,  "[Ar] 3d10 4s2 3p1", 1.81f,
     "Melts in your hand (29.8C).\nUsed in semiconductors, LEDs,\nand solar panels."},
    {32, "Ge", "Germanium",  72.63f,  14,4, CAT_METALLOID,  "[Ar] 3d10 4s2 3p2", 2.01f,
     "Semiconductor. Used in fiber\noptics, infrared optics, and\nearly transistors."},
    {33, "As", "Arsenic",    74.92f,  15,4, CAT_METALLOID,  "[Ar] 3d10 4s2 3p3", 2.18f,
     "Toxic metalloid. Historically\nused as poison. Used in\nsemiconductors (GaAs)."},
    {34, "Se", "Selenium",   78.97f,  16,4, CAT_NONMETAL,   "[Ar] 3d10 4s2 3p4", 2.55f,
     "Essential trace element.\nUsed in electronics, glass,\nand photocopiers."},
    {35, "Br", "Bromine",    79.90f,  17,4, CAT_HALOGEN,    "[Ar] 3d10 4s2 3p5", 2.96f,
     "Only liquid nonmetal at room\ntemperature. Red-brown, toxic.\nUsed in flame retardants."},
    {36, "Kr", "Krypton",    83.80f,  18,4, CAT_NOBLE_GAS,  "[Ar] 3d10 4s2 3p6", 0.00f,
     "Noble gas. Used in fluorescent\nlighting and photography\nflash equipment."},
};
#define PT_COUNT 36

// ---- Molecule definitions for 3D view ----

typedef struct {
    Vector3 pos;
    const char *symbol;
    Color color;
    float radius;
} MolAtom;

typedef struct {
    int a, b;    // atom indices
    int order;   // 1=single, 2=double, 3=triple
} MolBond;

typedef struct {
    const char *name;
    const char *formula;
    const char *description;
    MolAtom atoms[12];
    int atom_count;
    MolBond bonds[16];
    int bond_count;
} Molecule;

#define COL_C  (Color){80, 80, 80, 255}
#define COL_H  (Color){220, 220, 220, 255}
#define COL_O  (Color){220, 50, 50, 255}
#define COL_N  (Color){50, 80, 220, 255}
#define COL_CL (Color){50, 200, 50, 255}

static const Molecule molecules[] = {
    // Water H2O
    {"Water", "H2O",
     "Universal solvent. Bent geometry\n(104.5 deg). Polar molecule.\nEssential for all known life.",
     {
         {{0, 0, 0},       "O", COL_O, 0.30f},
         {{-0.8f, 0.6f, 0}, "H", COL_H, 0.20f},
         {{0.8f, 0.6f, 0},  "H", COL_H, 0.20f},
     }, 3,
     {{0,1,1}, {0,2,1}}, 2},

    // Carbon Dioxide CO2
    {"Carbon Dioxide", "CO2",
     "Linear molecule. Greenhouse gas.\nProduct of combustion and\nrespiration. Used in carbonation.",
     {
         {{0, 0, 0},       "C", COL_C, 0.28f},
         {{-1.2f, 0, 0},   "O", COL_O, 0.30f},
         {{1.2f, 0, 0},    "O", COL_O, 0.30f},
     }, 3,
     {{0,1,2}, {0,2,2}}, 2},

    // Methane CH4
    {"Methane", "CH4",
     "Simplest hydrocarbon. Tetrahedral\ngeometry. Natural gas main\ncomponent. Greenhouse gas.",
     {
         {{0, 0, 0},               "C", COL_C, 0.28f},
         {{0.9f, 0.9f, 0.0f},      "H", COL_H, 0.20f},
         {{-0.9f, -0.9f, 0.0f},    "H", COL_H, 0.20f},
         {{0.0f, 0.9f, -0.9f},     "H", COL_H, 0.20f},
         {{0.0f, -0.9f, 0.9f},     "H", COL_H, 0.20f},
     }, 5,
     {{0,1,1}, {0,2,1}, {0,3,1}, {0,4,1}}, 4},

    // Ammonia NH3
    {"Ammonia", "NH3",
     "Trigonal pyramidal. Pungent gas.\nUsed in fertilizers, cleaning\nproducts. Important industrial chemical.",
     {
         {{0, 0.3f, 0},            "N", COL_N, 0.28f},
         {{0.85f, -0.3f, 0.0f},    "H", COL_H, 0.20f},
         {{-0.42f, -0.3f, 0.73f},  "H", COL_H, 0.20f},
         {{-0.42f, -0.3f, -0.73f}, "H", COL_H, 0.20f},
     }, 4,
     {{0,1,1}, {0,2,1}, {0,3,1}}, 3},

    // Sodium Chloride NaCl
    {"Sodium Chloride", "NaCl",
     "Table salt. Ionic bond between\nNa+ and Cl-. Cubic crystal\nstructure. Essential mineral.",
     {
         {{-0.6f, 0, 0}, "Na", (Color){180, 100, 255, 255}, 0.35f},
         {{0.6f, 0, 0},  "Cl", COL_CL, 0.38f},
     }, 2,
     {{0,1,1}}, 1},

    // Ethanol C2H5OH
    {"Ethanol", "C2H5OH",
     "Drinking alcohol. Polar molecule.\nUsed as fuel, solvent, and in\nbeverages and hand sanitizer.",
     {
         {{-0.6f, 0, 0},           "C", COL_C, 0.28f},
         {{0.6f, 0, 0},            "C", COL_C, 0.28f},
         {{1.5f, 0.5f, 0},         "O", COL_O, 0.30f},
         {{2.2f, 0.2f, 0},         "H", COL_H, 0.20f},
         {{-0.6f, 0.9f, 0.4f},     "H", COL_H, 0.20f},
         {{-0.6f, -0.9f, 0.4f},    "H", COL_H, 0.20f},
         {{-1.3f, 0, -0.5f},       "H", COL_H, 0.20f},
         {{0.6f, 0.9f, -0.4f},     "H", COL_H, 0.20f},
         {{0.6f, -0.9f, -0.4f},    "H", COL_H, 0.20f},
     }, 9,
     {{0,1,1}, {1,2,1}, {2,3,1}, {0,4,1}, {0,5,1}, {0,6,1}, {1,7,1}, {1,8,1}}, 8},
};
#define MOL_COUNT 6

// ---- State ----

typedef enum {
    CHEM_PERIODIC_TABLE,
    CHEM_MOLECULE_VIEW,
} ChemView;

static ChemView  chem_view;
static int       selected_element;  // -1 = none, 0..PT_COUNT-1
static int       selected_molecule; // 0..MOL_COUNT-1
static float     chem_time;

// 3D camera for molecule view
static Camera3D  mol_cam;
static float     mol_orbit_angle;
static float     mol_orbit_pitch;
static float     mol_orbit_dist;
static bool      mol_orbiting;
static Vector2   mol_orbit_start;
static float     mol_orbit_angle0;
static float     mol_orbit_pitch0;

// Scroll for info panel
static float     chem_scroll;

#define SIDEBAR_W 340

static void chemistry_init(void) {
    chem_view          = CHEM_PERIODIC_TABLE;
    selected_element   = -1;
    selected_molecule  = 0;
    chem_time          = 0;
    mol_orbit_angle    = 0.6f;
    mol_orbit_pitch    = 0.3f;
    mol_orbit_dist     = 6.0f;
    mol_orbiting       = false;
    chem_scroll        = 0;

    mol_cam.target     = (Vector3){0, 0, 0};
    mol_cam.up         = (Vector3){0, 1, 0};
    mol_cam.fovy       = 45.0f;
    mol_cam.projection = CAMERA_PERSPECTIVE;
    mol_cam.position   = (Vector3){4, 3, 4};
}

static void update_mol_cam(void) {
    float cp = cosf(mol_orbit_pitch);
    mol_cam.position = (Vector3){
        mol_orbit_dist * cp * sinf(mol_orbit_angle),
        mol_orbit_dist * sinf(mol_orbit_pitch),
        mol_orbit_dist * cp * cosf(mol_orbit_angle)
    };
}

static void chemistry_update(Rectangle area) {
    chem_time += GetFrameTime();

    if (chem_view == CHEM_MOLECULE_VIEW) {
        Rectangle view3d = { area.x + SIDEBAR_W, area.y, area.width - SIDEBAR_W, area.height };
        Vector2 mouse = GetMousePosition();
        bool in_view = CheckCollisionPointRec(mouse, view3d);

        if (in_view && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            mol_orbiting     = true;
            mol_orbit_start  = mouse;
            mol_orbit_angle0 = mol_orbit_angle;
            mol_orbit_pitch0 = mol_orbit_pitch;
        }
        if (mol_orbiting) {
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                mol_orbit_angle = mol_orbit_angle0 - (mouse.x - mol_orbit_start.x) * 0.005f;
                mol_orbit_pitch = mol_orbit_pitch0 + (mouse.y - mol_orbit_start.y) * 0.005f;
                if (mol_orbit_pitch >  1.4f) mol_orbit_pitch =  1.4f;
                if (mol_orbit_pitch < -1.4f) mol_orbit_pitch = -1.4f;
            } else {
                mol_orbiting = false;
            }
        }
        if (in_view) {
            mol_orbit_dist -= GetMouseWheelMove() * 1.0f;
            if (mol_orbit_dist < 1.5f)  mol_orbit_dist = 1.5f;
            if (mol_orbit_dist > 30.0f) mol_orbit_dist = 30.0f;
        }
        if (IsKeyPressed(KEY_HOME)) {
            mol_orbit_angle = 0.6f;
            mol_orbit_pitch = 0.3f;
            mol_orbit_dist  = 6.0f;
        }
        update_mol_cam();
    }
}

// ---- Multiline text helper ----
static void draw_ml(const char *text, float x, float *y, int fsz, Color color) {
    const char *p = text;
    char line[256];
    while (*p) {
        int li = 0;
        while (*p && *p != '\n' && li < 254) line[li++] = *p++;
        line[li] = '\0';
        if (*p == '\n') p++;
        ui_draw_text(line, (int)x, (int)*y, fsz, color);
        *y += fsz + 3;
    }
}

// ---- Periodic Table Drawing ----

static void draw_periodic_table(Rectangle area) {
    DrawRectangleRec(area, COL_BG);

    float pad = 12;
    float avail_w = area.width - pad * 2;
    float avail_h = area.height - pad * 2 - 30; // leave room for title

    float cell_w = avail_w / 18.0f;
    float cell_h = avail_h / 7.0f;
    if (cell_w > 65) cell_w = 65;
    if (cell_h > 70) cell_h = 70;

    float table_w = cell_w * 18;
    float table_h = cell_h * 7;
    float ox = area.x + (area.width - table_w) / 2;
    float oy = area.y + pad + 28;

    // Title
    ui_draw_text("Periodic Table of Elements", (int)ox, (int)(area.y + pad), FONT_SIZE_LARGE, COL_ACCENT);

    Vector2 mouse = GetMousePosition();

    for (int i = 0; i < PT_COUNT; i++) {
        const PTElement *el = &periodic_table[i];
        int col = el->group - 1;  // 0-indexed
        int row = el->period - 1;

        float cx = ox + col * cell_w;
        float cy = oy + row * cell_h;
        Rectangle cell = { cx + 1, cy + 1, cell_w - 2, cell_h - 2 };

        bool hov = CheckCollisionPointRec(mouse, cell);
        bool sel = (selected_element == i);

        Color bg = cat_colors[el->cat];
        if (sel) {
            bg = (Color){bg.r, bg.g, bg.b, 255};
        } else if (hov) {
            bg = (Color){(unsigned char)(bg.r * 0.8f), (unsigned char)(bg.g * 0.8f),
                         (unsigned char)(bg.b * 0.8f), 220};
        } else {
            bg = (Color){(unsigned char)(bg.r * 0.4f), (unsigned char)(bg.g * 0.4f),
                         (unsigned char)(bg.b * 0.4f), 200};
        }

        DrawRectangleRounded(cell, 0.15f, 4, bg);
        if (sel) DrawRectangleRoundedLinesEx(cell, 0.15f, 4, 2.0f, WHITE);

        // Atomic number
        char zstr[8];
        snprintf(zstr, sizeof(zstr), "%d", el->Z);
        ui_draw_text(zstr, (int)(cx + 4), (int)(cy + 3), FONT_SIZE_TINY, (Color){255,255,255,180});

        // Symbol (centered, larger)
        int sym_w = ui_measure_text(el->symbol, FONT_SIZE_DEFAULT);
        ui_draw_text(el->symbol,
                     (int)(cx + (cell_w - sym_w) / 2),
                     (int)(cy + cell_h * 0.30f),
                     FONT_SIZE_DEFAULT, WHITE);

        // Mass
        if (cell_h > 45) {
            char mstr[16];
            snprintf(mstr, sizeof(mstr), "%.1f", el->mass);
            int mw = ui_measure_text(mstr, FONT_SIZE_TINY);
            ui_draw_text(mstr, (int)(cx + (cell_w - mw) / 2),
                         (int)(cy + cell_h - 14), FONT_SIZE_TINY, (Color){255,255,255,140});
        }

        if (hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            selected_element = i;
        }
    }

    // Category legend at bottom
    float ly = oy + table_h + 8;
    float lx = ox;
    for (int c = 0; c < CAT_COUNT; c++) {
        if (lx + 140 > ox + table_w) {
            lx = ox;
            ly += 18;
        }
        DrawRectangle((int)lx, (int)ly, 10, 10, cat_colors[c]);
        ui_draw_text(cat_names[c], (int)lx + 14, (int)ly - 1, FONT_SIZE_TINY, COL_TEXT_DIM);
        lx += 145;
    }

    // Selected element info panel (overlay at bottom-right)
    if (selected_element >= 0) {
        const PTElement *el = &periodic_table[selected_element];
        float pw = 300;
        float ph = 180;
        float px = area.x + area.width - pw - 12;
        float py = area.y + area.height - ph - 12;

        DrawRectangleRounded(
            (Rectangle){px, py, pw, ph}, 0.08f, 8,
            (Color){COL_PANEL.r, COL_PANEL.g, COL_PANEL.b, 240}
        );
        DrawRectangleRoundedLinesEx(
            (Rectangle){px, py, pw, ph}, 0.08f, 8, 1.5f, cat_colors[el->cat]
        );

        float iy = py + 8;

        // Symbol + name
        char title[64];
        snprintf(title, sizeof(title), "%s - %s", el->symbol, el->name);
        ui_draw_text(title, (int)(px + 12), (int)iy, FONT_SIZE_DEFAULT, cat_colors[el->cat]);
        iy += 24;

        char info[128];
        snprintf(info, sizeof(info), "Atomic Number: %d", el->Z);
        ui_draw_text(info, (int)(px + 12), (int)iy, FONT_SIZE_SMALL, COL_TEXT);
        iy += 18;

        snprintf(info, sizeof(info), "Atomic Mass: %.3f u", el->mass);
        ui_draw_text(info, (int)(px + 12), (int)iy, FONT_SIZE_SMALL, COL_TEXT);
        iy += 18;

        snprintf(info, sizeof(info), "Config: %s", el->electron_config);
        ui_draw_text(info, (int)(px + 12), (int)iy, FONT_SIZE_SMALL, COL_TEXT);
        iy += 18;

        if (el->electronegativity > 0.01f) {
            snprintf(info, sizeof(info), "Electronegativity: %.2f", el->electronegativity);
            ui_draw_text(info, (int)(px + 12), (int)iy, FONT_SIZE_SMALL, COL_TEXT);
            iy += 18;
        }

        snprintf(info, sizeof(info), "Category: %s", cat_names[el->cat]);
        ui_draw_text(info, (int)(px + 12), (int)iy, FONT_SIZE_SMALL, COL_TEXT_DIM);
        iy += 20;

        draw_ml(el->description, px + 12, &iy, FONT_SIZE_TINY, COL_TEXT_DIM);
    }
}

// ---- Molecule 3D Drawing ----

static void draw_bond_3d(Vector3 a, Vector3 b, int order, Color col) {
    if (order == 1) {
        DrawCylinderEx(a, b, 0.06f, 0.06f, 8, col);
    } else {
        // Offset bonds perpendicular to bond axis
        Vector3 dir = {b.x - a.x, b.y - a.y, b.z - a.z};
        float len = sqrtf(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
        if (len < 0.001f) return;

        // Find a perpendicular vector
        Vector3 up = {0, 1, 0};
        if (fabsf(dir.y / len) > 0.9f) up = (Vector3){1, 0, 0};
        // Cross product: perp = dir x up
        Vector3 perp = {
            dir.y * up.z - dir.z * up.y,
            dir.z * up.x - dir.x * up.z,
            dir.x * up.y - dir.y * up.x
        };
        float pl = sqrtf(perp.x*perp.x + perp.y*perp.y + perp.z*perp.z);
        if (pl < 0.001f) return;
        float offset = 0.08f;
        perp.x = perp.x / pl * offset;
        perp.y = perp.y / pl * offset;
        perp.z = perp.z / pl * offset;

        for (int i = 0; i < order; i++) {
            float f = (float)(i - (order - 1) * 0.5f);
            Vector3 oa = {a.x + perp.x * f, a.y + perp.y * f, a.z + perp.z * f};
            Vector3 ob = {b.x + perp.x * f, b.y + perp.y * f, b.z + perp.z * f};
            DrawCylinderEx(oa, ob, 0.04f, 0.04f, 6, col);
        }
    }
}

static void draw_molecule_3d(const Molecule *mol, float t) {
    (void)t;

    // Draw bonds
    for (int i = 0; i < mol->bond_count; i++) {
        const MolBond *b = &mol->bonds[i];
        draw_bond_3d(mol->atoms[b->a].pos, mol->atoms[b->b].pos,
                     b->order, (Color){150, 150, 160, 200});
    }

    // Draw atoms
    for (int i = 0; i < mol->atom_count; i++) {
        DrawSphere(mol->atoms[i].pos, mol->atoms[i].radius, mol->atoms[i].color);
        // Highlight ring
        DrawSphereWires(mol->atoms[i].pos, mol->atoms[i].radius + 0.01f, 6, 6,
                        (Color){255, 255, 255, 40});
    }
}

static void draw_molecule_view(Rectangle area) {
    // Sidebar
    Rectangle sidebar = { area.x, area.y, SIDEBAR_W, area.height };
    DrawRectangleRec(sidebar, COL_PANEL);
    DrawLine((int)(area.x + SIDEBAR_W), (int)area.y,
             (int)(area.x + SIDEBAR_W), (int)(area.y + area.height), COL_GRID);

    float sx = area.x + 8;
    float sw = SIDEBAR_W - 16;
    float sy = area.y + 8;

    // Back button
    {
        Rectangle back_btn = { sx, sy, 80, 26 };
        Vector2 mouse = GetMousePosition();
        bool hov = CheckCollisionPointRec(mouse, back_btn);
        DrawRectangleRounded(back_btn, 0.3f, 6, hov ? (Color){60,62,72,255} : COL_TAB);
        ui_draw_text("< Table", (int)sx + 10, (int)sy + 5, FONT_SIZE_SMALL,
                     hov ? COL_TEXT : COL_TEXT_DIM);
        if (hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            chem_view = CHEM_PERIODIC_TABLE;
            return;
        }
    }
    sy += 34;

    ui_draw_text("Molecules", (int)sx + 2, (int)sy, FONT_SIZE_LARGE, COL_ACCENT);
    sy += 32;

    // Molecule selector buttons
    for (int i = 0; i < MOL_COUNT; i++) {
        Rectangle btn = { sx, sy, sw, 28 };
        Vector2 mouse = GetMousePosition();
        bool hov = CheckCollisionPointRec(mouse, btn);
        bool sel = (i == selected_molecule);

        Color bg = sel ? COL_ACCENT : (hov ? (Color){50,52,62,255} : COL_TAB);
        DrawRectangleRounded(btn, 0.2f, 6, bg);

        char label[64];
        snprintf(label, sizeof(label), "%s (%s)", molecules[i].name, molecules[i].formula);
        Color fg = sel ? WHITE : (hov ? COL_TEXT : COL_TEXT_DIM);
        ui_draw_text(label, (int)sx + 10, (int)(sy + (28 - FONT_SIZE_SMALL) / 2),
                     FONT_SIZE_SMALL, fg);

        if (hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            selected_molecule = i;
        }
        sy += 32;
    }

    // Separator
    sy += 4;
    DrawLine((int)sx, (int)sy, (int)(sx + sw), (int)sy, COL_GRID);
    sy += 8;

    // Molecule info
    const Molecule *mol = &molecules[selected_molecule];

    char title[64];
    snprintf(title, sizeof(title), "%s  %s", mol->name, mol->formula);
    ui_draw_text(title, (int)sx + 2, (int)sy, FONT_SIZE_DEFAULT, COL_ACCENT);
    sy += 26;

    char stats[64];
    snprintf(stats, sizeof(stats), "Atoms: %d   Bonds: %d", mol->atom_count, mol->bond_count);
    ui_draw_text(stats, (int)sx + 2, (int)sy, FONT_SIZE_SMALL, COL_TEXT_DIM);
    sy += 22;

    DrawLine((int)sx, (int)sy, (int)(sx + sw), (int)sy, COL_GRID);
    sy += 8;

    draw_ml(mol->description, sx + 2, &sy, FONT_SIZE_TINY, COL_TEXT_DIM);

    sy += 12;

    // Atom legend
    ui_draw_text("Atom Colors:", (int)sx + 2, (int)sy, FONT_SIZE_SMALL, COL_TEXT_DIM);
    sy += 20;
    for (int i = 0; i < mol->atom_count; i++) {
        // De-duplicate symbols
        bool dup = false;
        for (int j = 0; j < i; j++) {
            if (strcmp(mol->atoms[j].symbol, mol->atoms[i].symbol) == 0) { dup = true; break; }
        }
        if (dup) continue;

        DrawCircle((int)(sx + 10), (int)(sy + 6), 5, mol->atoms[i].color);
        ui_draw_text(mol->atoms[i].symbol, (int)sx + 20, (int)sy, FONT_SIZE_TINY, COL_TEXT);
        sy += 16;
    }

    // Help
    ui_draw_text("Drag=Orbit  Scroll=Zoom  Home=Reset",
                 (int)sx, (int)(area.y + area.height - 20), FONT_SIZE_TINY, COL_TEXT_DIM);

    // ---- 3D Molecule view ----
    Rectangle view3d = { area.x + SIDEBAR_W, area.y, area.width - SIDEBAR_W, area.height };
    DrawRectangleRec(view3d, COL_BG);

    BeginScissorMode((int)view3d.x, (int)view3d.y, (int)view3d.width, (int)view3d.height);
    BeginMode3D(mol_cam);

    draw_molecule_3d(mol, chem_time);

    EndMode3D();
    EndScissorMode();

    // Molecule name overlay
    {
        char olabel[64];
        snprintf(olabel, sizeof(olabel), "%s  (%s)", mol->name, mol->formula);
        int tw = ui_measure_text(olabel, FONT_SIZE_DEFAULT);
        float ox = view3d.x + (view3d.width - tw) / 2;
        float oy = view3d.y + 10;
        DrawRectangleRounded(
            (Rectangle){ox - 8, oy - 4, (float)(tw + 16), (float)(FONT_SIZE_DEFAULT + 8)},
            0.3f, 6, (Color){COL_PANEL.r, COL_PANEL.g, COL_PANEL.b, 200}
        );
        ui_draw_text(olabel, (int)ox, (int)oy, FONT_SIZE_DEFAULT, COL_ACCENT);
    }

    // Atom labels in 3D view (projected to screen)
    for (int i = 0; i < mol->atom_count; i++) {
        Vector2 sp = GetWorldToScreen(mol->atoms[i].pos, mol_cam);
        if (sp.x > view3d.x && sp.x < view3d.x + view3d.width &&
            sp.y > view3d.y && sp.y < view3d.y + view3d.height) {
            int lw = ui_measure_text(mol->atoms[i].symbol, FONT_SIZE_TINY);
            DrawRectangleRounded(
                (Rectangle){sp.x - lw/2 - 3, sp.y - 20, (float)(lw + 6), 16},
                0.4f, 4, (Color){mol->atoms[i].color.r, mol->atoms[i].color.g,
                                  mol->atoms[i].color.b, 180}
            );
            ui_draw_text(mol->atoms[i].symbol, (int)(sp.x - lw/2), (int)(sp.y - 19), FONT_SIZE_TINY, WHITE);
        }
    }
}

// ---- View switcher button in periodic table ----

static void draw_view_switcher(Rectangle area) {
    float bx = area.x + area.width - 140;
    float by = area.y + 12;
    Rectangle btn = { bx, by, 128, 28 };
    Vector2 mouse = GetMousePosition();
    bool hov = CheckCollisionPointRec(mouse, btn);

    DrawRectangleRounded(btn, 0.3f, 6, hov ? COL_ACCENT : COL_TAB);
    const char *label = "3D Molecules >";
    int tw = ui_measure_text(label, FONT_SIZE_SMALL);
    ui_draw_text(label, (int)(bx + (128 - tw) / 2), (int)(by + 6),
                 FONT_SIZE_SMALL, hov ? WHITE : COL_TEXT_DIM);

    if (hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        chem_view = CHEM_MOLECULE_VIEW;
    }
}

static void chemistry_draw(Rectangle area) {
    if (chem_view == CHEM_MOLECULE_VIEW) {
        draw_molecule_view(area);
    } else {
        draw_periodic_table(area);
        draw_view_switcher(area);
    }
}

static void chemistry_cleanup(void) {
    // Nothing to free
}

static Module chemistry_mod = {
    .name    = "Chemistry",
    .init    = chemistry_init,
    .update  = chemistry_update,
    .draw    = chemistry_draw,
    .cleanup = chemistry_cleanup,
};

Module *chemistry_module(void) {
    return &chemistry_mod;
}
