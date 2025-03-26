#ifndef gen_impl_H
#define gen_impl_H

#ifndef IMPLEMENT_FLAG
#define IMPLEMENT_FLAG
#define gen_impl_C
#endif

#include <stdio.h>

#define write     gen_write
#define set_fs    gen_set_fs
#define close_fs  gen_close_fs

void write(char *msg, ...);
void set_fs(FILE *_fs);
void close_fs();

#endif
#ifdef gen_impl_C

#include "print.c"

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

#endif