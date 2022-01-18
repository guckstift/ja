#include <stdio.h>
#include <stdlib.h>
#include "gen.h"
#include "print.h"

#define COL_YELLOW  "\x1b[38;2;255;255;0m"
#define COL_RESET   "\x1b[0m"

static void error(char *msg)
{
	print_error(0, 0, 0, 0, msg);
	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	if(argc < 2) error("no input file");
	if(argc > 2) error("too many input files");
	char *filename = argv[1];
	
	FILE *fs = fopen(filename, "rb");
	if(!fs) error("can not open input file");
	fseek(fs, 0, SEEK_END);
	int64_t src_len = ftell(fs);
	rewind(fs);
	char *src = malloc(src_len + 1);
	src[src_len] = 0;
	fread(src, 1, src_len, fs);
	fclose(fs);
	
	Tokens *tokens = lex(src, src_len);
	print_tokens(tokens);
	
	Unit *unit = parse(tokens);
	print_unit(unit);
	
	printf(COL_YELLOW "=== code ===" COL_RESET "\n");
	gen(unit);
	
	return 0;
}
