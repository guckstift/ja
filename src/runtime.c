#include "runtime.h"

jastring ja_read_file(jastring filename)
{
	FILE *fs = fopen(filename.string, "rb");
	if(!fs) return (jastring){0};
	fseek(fs, 0, SEEK_END);
	int64_t len = ftell(fs);
	rewind(fs);
	char *text = malloc(len + 1);
	text[len] = 0;
	fread(text, 1, len, fs);
	fclose(fs);
	return (jastring){.length = len, .string = text};
}
