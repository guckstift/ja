#define RUNTIME_C_SRC \
	"#include \"runtime.h\"\n" \
	"\n" \
	"jastring ja_read_file(jastring filename)\n" \
	"{\n" \
	"	FILE *fs = fopen(filename.string, \"rb\");\n" \
	"	if(!fs) return (jastring){0};\n" \
	"	fseek(fs, 0, SEEK_END);\n" \
	"	int64_t len = ftell(fs);\n" \
	"	rewind(fs);\n" \
	"	char *text = malloc(len + 1);\n" \
	"	text[len] = 0;\n" \
	"	fread(text, 1, len, fs);\n" \
	"	fclose(fs);\n" \
	"	return (jastring){.length = len, .string = text};\n" \
	"}\n" \

