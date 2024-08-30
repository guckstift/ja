#include "array.h"
#include "arena.h"

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