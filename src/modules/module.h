#ifndef MODULE_H
#define MODULE_H

#include "raylib.h"

typedef struct Module {
    const char *name;
    const char *help_text;
    void (*init)(void);
    void (*update)(Rectangle area);
    void (*draw)(Rectangle area);
    void (*cleanup)(void);
} Module;

#endif
