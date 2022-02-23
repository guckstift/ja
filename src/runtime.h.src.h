#define RUNTIME_H_SRC \
	"#ifndef JA_RUNTIME_H\n" \
	"#define JA_RUNTIME_H\n" \
	"\n" \
	"#include <stdio.h>\n" \
	"#include <stdint.h>\n" \
	"#include <inttypes.h>\n" \
	"#include <string.h>\n" \
	"#include <stdlib.h>\n" \
	"#include <dlfcn.h>\n" \
	"\n" \
	"#define jafalse ((jabool)0)\n" \
	"#define jatrue ((jabool)1)\n" \
	"\n" \
	"typedef uint8_t jabool;\n" \
	"\n" \
	"typedef struct {\n" \
	"	int64_t length;\n" \
	"	void *items;\n" \
	"} jadynarray;\n" \
	"\n" \
	"typedef struct {\n" \
	"	int64_t length;\n" \
	"	char *string;\n" \
	"} jastring;\n" \
	"\n" \
	"jastring ja_read(jastring filename);\n" \
	"\n" \
	"#endif\n" \

