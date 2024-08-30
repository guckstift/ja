#include <stdio.h>
#include <stdlib.h>
#include "build.h"
#include "error.h"

#ifndef JA_TEST

int main(int argc, char **argv)
{
	if(argc < 2) {
		error("no input file specified");
		exit(EXIT_FAILURE);
	}

	char *mainfile = argv[1];
	build(mainfile);

	return 0;
}

#endif