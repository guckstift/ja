#ifndef STRING_H
#define STRING_H

#include <string.h>
#include "array.h"

#define string_length(s)  ( \
	(s) \
		? ((uint64_t*)(s))[-1] - 1 \
		: 0 \
)

#define string_clone(s) \
	malloc(strlen(s) + 1 + sizeof)

#define string_append(d, s)  do { \
	uint64_t oldlen = string_length(d); \
	uint64_t pluslen = strlen(s); \
	array_resize((d), oldlen + pluslen + 1); \
	memcpy((d) + oldlen, (s), pluslen); \
	(d)[oldlen + pluslen] = 0; \
} while(0)

#define string_append_token(d, t)  do { \
	uint64_t oldlen = string_length(d); \
	uint64_t pluslen = (t)->length; \
	array_resize((d), oldlen + pluslen + 1); \
	memcpy((d) + oldlen, (t)->start, pluslen); \
	(d)[oldlen + pluslen] = 0; \
} while(0)

#endif
