#ifndef ARRAY_H
#define ARRAY_H

#include <stdint.h>
#include <stdlib.h>

#define array_length(a)     _array_length((void*)a)
#define array_resize(a, l)  (a = _array_resize((void*)a, l, sizeof*(a)))
#define array_grow(a, p)    (a = _array_grow((void*)a, p, sizeof*(a)))
#define array_push(a, v)    (array_grow(a, 1), (a)[((uint64_t*)a)[-1] - 1] = v)
#define array_for(a, i)     for(uint64_t i = 0; i < array_length(a); i++)

static inline uint64_t _array_length(uint64_t *a)
{
	return a ? a[-1] : 0;
}

static inline void *_array_resize(uint64_t *a, uint64_t l, uint64_t s)
{
	a && a--;
	a = realloc(a, l * s + 8);
	*a = l;
	return a + 1;
}

static inline void *_array_grow(uint64_t *a, uint64_t p, uint64_t s)
{
	return _array_resize(a, _array_length(a) + p, s);
}

#endif
