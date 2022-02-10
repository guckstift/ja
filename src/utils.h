#ifndef UTILS_H
#define UTILS_H

#include "lex.h"

#define list_push(owner, first, last, next, item) do { \
	if(owner->first) { \
		owner->last->next = item; \
		owner->last = item; \
	} \
	else { \
		owner->first = item; \
		owner->last = item; \
	} \
} while(0)

#define headless_list_push(first, last, next, item) do { \
	if(first) { \
		last->next = item; \
		last = item; \
	} \
	else { \
		first = item; \
		last = item; \
	} \
} while(0)

#define for_list(T, iter, start, next) \
	for(T *iter = start; iter; iter = iter->next)

#define str_append(dest, src) (dest) = _str_append(dest, src)
#define str_append_token(dest, src) (dest) = _str_append_token(dest, src)

char *_str_append(char *dest, char *app);
char *_str_append_token(char *dest, Token *token);

#endif
