#ifndef PLOTTER3D_H
#define PLOTTER3D_H

#include "raylib.h"
#include "parser.h"
#include "plotter.h"
#include "../../utils/arena.h"

// 3D vector entry
#define MAX_VECTORS 16
#define VEC_BUF_SIZE 128

typedef struct {
    float x, y, z;
    int   color_idx;
    char  label[32];
    char  expr[VEC_BUF_SIZE]; // original text e.g. "1,2,3"
    bool  visible;
} VecEntry;

typedef struct {
    // Camera
    Camera3D camera;
    float    orbit_angle;  // horizontal orbit angle (radians)
    float    orbit_pitch;  // vertical orbit angle (radians)
    float    orbit_dist;   // distance from target
    bool     orbiting;
    Vector2  orbit_start;
    float    orbit_angle0;
    float    orbit_pitch0;

    // Surface functions (z = f(x,y))
    FuncSlot  surfs[MAX_FUNCTIONS];
    int       surf_count;

    // Vectors
    VecEntry  vecs[MAX_VECTORS];
    int       vec_count;

    // View range
    float     range; // half-extent of x/y axes
} Plot3DState;

void plotter3d_init(Plot3DState *ps);
void plotter3d_update(Plot3DState *ps, Rectangle area);
void plotter3d_draw(Plot3DState *ps, Rectangle area, Arena *arena);

#endif
