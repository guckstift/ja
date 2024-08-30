#ifndef ARENA_H
#define ARENA_H

#include <stdint.h>

void *alloc(int64_t size);
void *resize(void *ptr, int64_t size, int64_t oldsize);
void free_arena();
int64_t get_total_alloc();

#endif