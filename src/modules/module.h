#ifndef MODULE_H
#define MODULE_H

#include "raylib.h"

typedef struct Module {
    const char *name;
    void (*init)(void);
    void (*update)(Rectangle area);
    void (*draw)(Rectangle area);
    void (*cleanup)(void);
} Module;

#endif
