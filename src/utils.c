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
	return dest;
}
