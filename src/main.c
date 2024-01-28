#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dlfcn.h>
#include "print.h"
#include "build.h"
#include "utils/string.h"

static bool compile_only = false;
static int prog_argc = 0;
static char **prog_argv = 0;
static BuildOptions build_options = {0};

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
			compile_only = true;
		}
		else if(strcmp(argv[i], "-st") == 0) {
			build_options.show_tokens = true;
		}
		else if(strcmp(argv[i], "-sa") == 0) {
			build_options.show_ast = true;
		}
		else if(compile_only && build_options.outfilename == 0) {
			build_options.outfilename = argv[i];
		}
		else if(build_options.main_filename == 0) {
			build_options.main_filename = argv[i];
			prog_argc = argc - i;
			prog_argv = argv + i;
			break;
		}
	}

	if(compile_only && build_options.outfilename == 0)
		error("no output filename");

	if(build_options.main_filename == 0)
		error("no input file");
}

int main(int argc, char *argv[])
{
	#ifdef JA_DEBUG
	build_options.show_tokens = true;
	build_options.show_ast = true;
	#endif

	parse_args(argc, argv);
	Project *project = build(build_options);

	if(!compile_only) {
		char *cmd = 0;
		string_append(cmd, project->exe_filename);

		for(int64_t i=1; i < prog_argc; i++) {
			string_append(cmd, " ");
			string_append(cmd, prog_argv[i]);
		}

		#ifdef JA_DEBUG
		printf(COL_YELLOW "[run]:" COL_RESET " %s\n", cmd);
		#endif

		system(cmd);
	}

	return 0;
}