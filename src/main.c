
#define IMPLEMENT_FLAG

#include <stdio.h>
#include <stdlib.h>
#include "build.c"
#include "error.c"

int main(int argc, char **argv)
{
	if(argc < 2) {
		error("no input file specified");
		exit(EXIT_FAILURE);
	}

	char *mainfile = argv[1];
	Project *project = build(mainfile);

	if(!project)
		exit(EXIT_FAILURE);

	system(project->progfile);

	return 0;
}