#include "arena.h"
#include <stdlib.h>
#include <string.h>

Arena arena_create(size_t cap) {
    Arena a;
    a.buf  = malloc(cap);
    a.cap  = cap;
    a.used = 0;
    return a;
}

void *arena_alloc(Arena *a, size_t size) {
    // align to 8 bytes
    size_t aligned = (size + 7) & ~(size_t)7;
    if (a->used + aligned > a->cap) return NULL;
    void *ptr = a->buf + a->used;
    a->used += aligned;
    return ptr;
}

void arena_reset(Arena *a) {
    a->used = 0;
}

void arena_destroy(Arena *a) {
    free(a->buf);
    a->buf  = NULL;
    a->cap  = 0;
    a->used = 0;
}
