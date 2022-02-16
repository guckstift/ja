#ifndef ARRAY_H
#define ARRAY_H

#include <stdint.h>
#include <stdlib.h>

#define array_base_ptr(a) ((a) ? ((int64_t*)(a) - 1) : 0)
#define array_length(a) ((a) ? *array_base_ptr(a) : 0)

#define array_resize(a, l) do { \
	(a) = (void*)((int64_t*)realloc( \
		array_base_ptr(a), \
		sizeof(int64_t) + (l) * sizeof(*a) \
	) + 1); \
	*array_base_ptr(a) = (l); \
} while(0)

#define array_push(a, x) do { \
	array_resize(a, array_length(a) + 1); \
	(a)[array_length(a) - 1] = (x); \
} while(0)

#define array_for(a, i) \
	for(int64_t i = 0; i < array_length(a); i++)

#endif
