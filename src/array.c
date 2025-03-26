#ifndef array_H
#define array_H

#ifndef IMPLEMENT_FLAG
#define IMPLEMENT_FLAG
#define array_C
#endif

#include <stdint.h>

#define array_length(array)         ((array) ? ((int64_t*)array)[-1] : 0)
#define array_grow(array)           ((array) = _array_grow((void*)array, sizeof*(array)))
#define array_resize(array, length) ((array) = _array_resize((void*)array, sizeof*(array), length))
#define array_push(array, val)      (array_grow(array), (array)[array_length(array) - 1] = (val))
#define array_for(array, i)         for(int64_t i = 0; i < array_length(array); i ++)

void *_array_grow(int64_t *array, int64_t itemsize);
void *_array_resize(int64_t *array, int64_t itemsize, int64_t length);

#endif
#ifdef array_C

#include "arena.c"

void *_array_grow(int64_t *array, int64_t itemsize)
{
	int64_t oldlen = array ? array[-1] : 0;
	return _array_resize(array, itemsize, oldlen + 1);
}

void *_array_resize(int64_t *array, int64_t itemsize, int64_t length)
{
	int64_t oldlen = array ? array[-1] : 0;
	int64_t oldsize = oldlen ? oldlen * itemsize + sizeof(int64_t) : 0;
	int64_t size = length * itemsize + sizeof(int64_t);
	void *ptr = array ? array - 1 : 0;
	array = resize(ptr, size, oldsize);
	*array = length;
	return array + 1;
}

#endif