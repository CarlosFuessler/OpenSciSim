#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>

#define ARENA_DEFAULT_CAP (1024 * 64) // 64 KB

typedef struct Arena {
    char  *buf;
    size_t cap;
    size_t used;
} Arena;

Arena  arena_create(size_t cap);
void  *arena_alloc(Arena *a, size_t size);
void   arena_reset(Arena *a);
void   arena_destroy(Arena *a);

#endif
