#ifndef runtime_H
#define runtime_H

#include <stdint.h>

typedef uint8_t jabool;

typedef struct {
	void *items;
	uint64_t length;
} jaslice;

typedef struct {
	char *chars;
	uint64_t length;
} jastring;

void print_int(int64_t i);
void print_uint(uint64_t u);
void print_bool(jabool b);
void print_string(jastring s);

#endif