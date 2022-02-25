#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "print.h"
#include "build.h"
#include "utils.h"

#define COL_YELLOW  "\x1b[38;2;255;255;0m"
#define COL_RESET   "\x1b[0m"

static int compile_only = 0;
static char *outfilename = 0;
static char *filename = 0;
static int prog_argc = 0;
static char **prog_argv = 0;

static void error(char *msg, ...)
{
	va_list args;
	va_start(args, msg);
	vprint_error(0, 0, 0, 0, msg, args);
	va_end(args);
	exit(EXIT_FAILURE);
}

static void parse_args(int argc, char **argv)
{
	for(int64_t i=1; i < argc; i++) {
		if(strcmp(argv[i], "-c") == 0) {
			compile_only = 1;
		}
		else if(compile_only == 1 && outfilename == 0) {
			outfilename = argv[i];
		}
		else if(filename == 0) {
			filename = argv[i];
			prog_argc = argc - i;
			prog_argv = argv + i;
			break;
		}
	}
	
	if(compile_only == 1 && outfilename == 0) error("no output filename");
	if(filename == 0) error("no input file");
}

int main(int argc, char *argv[])
{
	parse_args(argc, argv);
	Project *project = build(filename, outfilename);
	
	if(!compile_only) {
		char *cmd = 0;
		str_append(cmd, project->exe_filename);
		
		for(int64_t i=1; i < prog_argc; i++) {
			str_append(cmd, " ");
			str_append(cmd, prog_argv[i]);
		}
		
		#ifdef JA_DEBUG
		printf(COL_YELLOW "[run]:" COL_RESET " %s\n", cmd);
		#endif
		
		system(cmd);
	}
	
	return 0;
}
