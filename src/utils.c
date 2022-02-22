#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "utils.h"

char *_str_append(char *dest, char *app)
{
	uint64_t newlen = (dest ? strlen(dest) : 0) + strlen(app);
	if(dest) {
		dest = realloc(dest, newlen + 1);
		strcat(dest, app);
	}
	else {
		dest = malloc(newlen + 1);
		strcpy(dest, app);
	}
	return dest;
}

char *_str_append_token(char *dest, Token *token)
{
	uint64_t oldlen = dest ? strlen(dest) : 0;
	uint64_t newlen = oldlen + token->length;
	if(dest) {
		dest = realloc(dest, newlen + 1);
		memcpy(dest + oldlen, token->start, token->length);
	}
	else {
		dest = malloc(newlen + 1);
		memcpy(dest, token->start, token->length);
	}
	dest[newlen] = 0;
	return dest;
}

int64_t array_length_get(void *array)
{
	if(!array) return 0;
	return ((int64_t*)array)[-1];
}

void *array_resize_impl(void *array, int64_t length, int64_t itemsize)
{
	void *base = 0;
	if(array) base = ((int64_t*)array) - 1;
	base = realloc(base, sizeof(int64_t) + length * itemsize);
	array = ((int64_t*)base) + 1;
	*(int64_t*)base = length;
	return array;
}
