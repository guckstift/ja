#ifndef STRING_H
#define STRING_H

#include <stdint.h>
#include "array.h"
#include "ast.h"

#define string_length(string)          ((string) ? ((int64_t*)string)[-1] - 1 : 0)
#define string_append(dest, src)       ((dest) = _string_append(dest, src))
#define string_append_token(dest, src) ((dest) = _string_append_token(dest, src))

char *_string_append(char *dest, char *src);
char *_string_append_token(char *dest, Token *src);
char *string_from_cstr(char *cstr);
char *string_concat(char *first, ...);

#endif