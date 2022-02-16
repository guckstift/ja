#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stdlib.h>
#include "lex.h"

#define str_append(dest, src) (dest) = _str_append(dest, src)
#define str_append_token(dest, src) (dest) = _str_append_token(dest, src)

#define array_length(a) (array_length_get(a))
#define array_resize(a, l) ((a) = array_resize_impl((a), (l), sizeof(*a)))
#define array_last(a) ((a)[array_length(a) - 1])

#define array_for(a, i) \
	for(int64_t i = 0; i < array_length(a); i++)

#define array_push(a, x) do { \
	array_resize((a), array_length(a) + 1); \
	array_last(a) = (x); \
} while(0)

char *_str_append(char *dest, char *app);
char *_str_append_token(char *dest, Token *token);

int64_t array_length_get(void *array);
void *array_resize_impl(void *array, int64_t length, int64_t itemsize);

#endif
