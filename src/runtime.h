#ifndef JA_RUNTIME_H
#define JA_RUNTIME_H

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>

#define jafalse ((jabool)0)
#define jatrue ((jabool)1)

typedef uint8_t jabool;

typedef struct {
	int64_t length;
	void *items;
} jaslice;

typedef struct {
	int64_t length;
	char *string;
} jastring;

jastring ja_read(jastring filename);

#endif
