#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "string.h"
#include "arena.h"

char *_string_append(char *dest, char *src)
{
	int64_t oldlength = string_length(dest);
	int64_t pluslength = strlen(src) + 1;
	array_resize(dest, oldlength + pluslength);
	memcpy(dest + oldlength, src, pluslength);
	return dest;
}

char *_string_append_token(char *dest, Token *src)
{
	int64_t oldlength = string_length(dest);
	array_resize(dest, oldlength + src->length + 1);
	memcpy(dest + oldlength, src->start, src->length);
	dest[oldlength + src->length] = 0;
	return dest;
}

char *string_from_cstr(char *cstr)
{
	int64_t length = strlen(cstr) + 1;
	int64_t *res = alloc(sizeof(int64_t) + length);
	*res = length;
	return memcpy(res + 1, cstr, length);
}

char *string_concat(char *first, ...)
{
	va_list args;
	va_start(args, first);
	char *res = string_from_cstr(first);

	while(1) {
		char *cstr = va_arg(args, char*);

		if(cstr == 0)
			break;

		string_append(res, cstr);
	}

	va_end(args);
	return res;
}