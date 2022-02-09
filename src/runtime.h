#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#define jafalse ((jabool)0)
#define jatrue ((jabool)1)

typedef uint8_t jabool;

typedef struct {
	int64_t length;
	void *items;
} jadynarray;

typedef struct {
	int64_t length;
	char *string;
} jastring;

