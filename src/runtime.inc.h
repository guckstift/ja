#define RUNTIME_H_SRC \
	"#include <stdio.h>\n" \
	"#include <stdint.h>\n" \
	"#include <inttypes.h>\n" \
	"#include <string.h>\n" \
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

