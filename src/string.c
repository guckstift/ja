#include <stdarg.h>
#include "string.h"

char *string_clone(char *src)
{
	uint64_t len = strlen(src);
	uint64_t *block = malloc(sizeof(uint64_t) + len + 1);
	block[0] = len + 1;
	char *res = (char*)(block + 1);
	strcpy(res, src);
	return res;
}

char *string_concat(char *first, ...)
{
	va_list args;
	va_start(args, first);
	char *res = string_clone(first);
	
	while(1) {
		char *cstr = va_arg(args, char*);
		if(cstr == 0) break;
		string_append(res, cstr);
	}
	
	va_end(args);
	return res;
}
