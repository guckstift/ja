#ifndef ARRAY_H
#define ARRAY_H

#include <stdint.h>
#include <stdlib.h>

#define array_length(a)  ( \
	(a) \
		? ((uint64_t*)(a))[-1] \
		: 0 \
)

#define array_last(a)  ( \
	array_length(a) \
		? ((a) + ((uint64_t*)(a))[-1] - 1) \
		: 0 \
)

#define array_resize(a, l)  ( \
	(a) = (void*)( \
		(uint64_t*)realloc( \
			(a) \
				? (uint64_t*)(a) - 1 \
				: 0 \
			, (l) * sizeof*(a) + sizeof(uint64_t) \
		) + 1 \
	), \
	((uint64_t*)(a))[-1] = (l) \
)

#define array_push(a, v)  do { \
	uint64_t oldlen = array_length(a); \
	array_resize(a, oldlen + 1); \
	(a)[oldlen] = (v); \
} while(0)

#define array_for(a, i) \
	for(uint64_t i = 0; i < array_length(a); i++)

#endif
