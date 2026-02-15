#ifndef THEME_H
#define THEME_H

#include "raylib.h"

// Window
#define WINDOW_W 1280
#define WINDOW_H 800
#define TARGET_FPS 60
#define UI_MIN_SCALE 0.75f
#define UI_MAX_SCALE 1.5f

// Font
#define FONT_PATH "assets/fonts/JetBrainsMono-Regular.ttf"
#define FONT_LOAD_SIZE 128
#define FONT_SIZE_DEFAULT 18
#define FONT_SIZE_SMALL   15
#define FONT_SIZE_LARGE   26
#define FONT_SIZE_TITLE   48
#define FONT_SIZE_HERO    72
#define FONT_SIZE_TINY    13

// Tab bar
#define TAB_HEIGHT 44
#define TAB_PADDING 20
#define TAB_FONT_SIZE FONT_SIZE_DEFAULT

// Colors
#define COL_BG         (Color){ 24,  24,  28, 255 }
#define COL_PANEL      (Color){ 32,  34,  40, 255 }
#define COL_TAB        (Color){ 42,  44,  52, 255 }
#define COL_TAB_ACT    (Color){ 56, 120, 220, 255 }
#define COL_TEXT        (Color){225, 225, 230, 255 }
#define COL_TEXT_DIM    (Color){120, 122, 135, 255 }
#define COL_ACCENT     (Color){ 56, 120, 220, 255 }
#define COL_ACCENT2    (Color){ 80, 200, 160, 255 }
#define COL_GRID       (Color){ 48,  50,  58, 255 }
#define COL_AXIS       (Color){140, 142, 155, 255 }
#define COL_INPUT_BG   (Color){ 38,  40,  48, 255 }
#define COL_ERROR      (Color){230,  75,  75, 255 }
#define COL_SUCCESS    (Color){ 80, 200, 120, 255 }

// Plot colors for multiple functions
static const Color PLOT_COLORS[] = {
    { 66, 165, 245, 255 },  // blue
    {239,  83,  80, 255 },  // red
    {102, 187, 106, 255 },  // green
    {255, 202,  40, 255 },  // yellow
    {171, 71,  188, 255 },  // purple
    {255, 167,  38, 255 },  // orange
    { 38, 198, 218, 255 },  // teal
    {236, 64,  122, 255 },  // pink
};
#define PLOT_COLOR_COUNT 8

// Global font (set in main.c)
extern Font g_font;

#endif
