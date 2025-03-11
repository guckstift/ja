#include <stdio.h>
#include <inttypes.h>
#include "runtime.h"

void print_int(int64_t i)
{
	printf("%" PRId64, i);
}

void print_uint(uint64_t u)
{
	printf("%" PRIu64, u);
}

void print_bool(jabool b)
{
	printf("%s", b ? "true" : "false");
}

void print_string(jastring s)
{
	fwrite(s.chars, 1, s.length, stdout);
}
