#include <stdio.h>
#include <stdlib.h>
#include "lex.h"
#include "print.h"

static void error(char *msg)
{
	fprintf(stderr, "error: %s\n", msg);
	exit(EXIT_FAILURE);
}

static char *load(char *filename)
{
	FILE *fs = fopen(filename, "rb");
	if(!fs) error("can not open input file");
	fseek(fs, 0, SEEK_END);
	long length = 0;
	length = ftell(fs);
	rewind(fs);
	char *src = malloc(length + 1);
	src[length] = 0;
	fread(src, 1, length, fs);
	fclose(fs);
	return src;
}

int main(int argc, char *argv[])
{
	if(argc < 2) error("no input file");
	if(argc > 2) error("too many input files");
	char *filename = argv[1];
	
	char *src = load(filename);
	
	Token *tokens = lex(src);
	print_tokens(tokens);
	
	return 0;
}
