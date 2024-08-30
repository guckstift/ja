#include "gen_impl.h"
#include "print.h"

static FILE *fs;

void write(char *msg, ...)
{
	va_list args;
	va_start(args, msg);
	vfprint(fs, msg, args);
	va_end(args);
}

void set_fs(FILE *_fs)
{
	fs = _fs;
}

void close_fs()
{
	fclose(fs);
}