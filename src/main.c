#include <stdio.h>
#include <stdlib.h>
#include "print.h"
#include "build.h"

static void error(char *msg, ...)
{
	va_list args;
	va_start(args, msg);
	vprint_error(0, 0, 0, 0, msg, args);
	va_end(args);
	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	if(argc < 2) error("no input file");
	if(argc > 2) error("too many input files");
	char *filename = argv[1];
	
	build(filename);
	
	return 0;
}
