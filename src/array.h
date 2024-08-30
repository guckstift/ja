#ifndef ARRAY_H
#define ARRAY_H

#include <stdint.h>

#define array_length(array)         ((array) ? ((int64_t*)array)[-1] : 0)
#define array_grow(array)           ((array) = _array_grow((void*)array, sizeof*(array)))
#define array_resize(array, length) ((array) = _array_resize((void*)array, sizeof*(array), length))
#define array_push(array, val)      (array_grow(array), (array)[array_length(array) - 1] = (val))
#define array_for(array, i)         for(int64_t i = 0; i < array_length(array); i ++)

void *_array_grow(int64_t *array, int64_t itemsize);
void *_array_resize(int64_t *array, int64_t itemsize, int64_t length);

#endif