#ifndef string_H
#define string_H

#ifndef IMPLEMENT_FLAG
#define IMPLEMENT_FLAG
#define string_C
#endif

#include <stdint.h>
#include "array.c"
#include "ast.c"

#define string_length(string)          ((string) ? ((int64_t*)string)[-1] - 1 : 0)
#define string_append(dest, src)       ((dest) = _string_append(dest, src))
#define string_append_ch(dest, ch)     ((dest) = _string_append_ch(dest, ch))
#define string_append_token(dest, src) ((dest) = _string_append_token(dest, src))

char *_string_append(char *dest, char *src);
char *_string_append_ch(char *dest, char ch);
char *_string_append_token(char *dest, Token *src);
char *string_from_cstr(char *cstr);
char *string_concat(char *first, ...);
char *string_prefix(char *src, uint64_t length);

#endif
#ifdef string_C

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "arena.c"

char *_string_append(char *dest, char *src)
{
	int64_t oldlength = string_length(dest);
	int64_t pluslength = strlen(src) + 1;
	array_resize(dest, oldlength + pluslength);
	memcpy(dest + oldlength, src, pluslength);
	return dest;
}

char *_string_append_ch(char *dest, char ch)
{
	int64_t oldlength = string_length(dest);
	array_resize(dest, oldlength + 2);
	dest[oldlength] = ch;
	dest[oldlength + 1] = 0;
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

char *string_prefix(char *src, uint64_t length)
{
	char *end = src;
	uint64_t str_len = strlen(src);
	if(length > str_len) length = str_len;
	char *res = 0;
	array_resize(res, length + 1);
	memcpy(res, src, length);
	res[length] = 0;
	return res;
}

#endif