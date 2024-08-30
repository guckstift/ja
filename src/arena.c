#include <stdlib.h>
#include <string.h>
#include "arena.h"

typedef struct {
	uint8_t *start;
	uint8_t *end;
	uint8_t *cur;
	void *prev;
} Arena;

static Arena *arena = 0;
static int64_t new_arena_size = 1024 * 1024;
static int64_t last_alloc_size = 0;
static int64_t total_alloc = 0;

static void add_arena(int64_t needed)
{
	while(needed > new_arena_size)
		new_arena_size *= 2;

	Arena *new_arena = calloc(1, sizeof(Arena) + new_arena_size);
	void *start = new_arena + 1;
	new_arena->start = start;
	new_arena->end = new_arena->start + new_arena_size;
	new_arena->cur = new_arena->start;
	new_arena->prev = arena;
	arena = new_arena;
	last_alloc_size = 0;
}

static int64_t available()
{
	return arena ? arena->end - arena->cur : 0;
}

static void *last_alloc()
{
	return arena ? arena->cur - last_alloc_size : 0;
}

static void free_last()
{
	arena->cur -= last_alloc_size;
	total_alloc -= last_alloc_size;
	last_alloc_size = 0;
}

void *alloc(int64_t size)
{
	size = (size + 7) & ~7;

	if(size > available())
		add_arena(size);

	void *ptr = arena->cur;
	arena->cur += size;
	last_alloc_size = size;
	total_alloc += size;
	return ptr;
}

void *resize(void *ptr, int64_t size, int64_t oldsize)
{
	if(ptr && ptr == last_alloc())
		free_last();

	void *new_ptr = alloc(size);

	if(ptr && new_ptr != ptr)
		memcpy(new_ptr, ptr, oldsize < size ? oldsize : size);

	return new_ptr;
}

void free_arena()
{
	while(arena) {
		Arena *prev = arena->prev;
		free(arena);
		arena = prev;
	}

	last_alloc_size = 0;
	total_alloc = 0;
}

int64_t get_total_alloc()
{
	return total_alloc;
}