#ifndef GEN_IMPL_H
#define GEN_IMPL_H

#include <stdio.h>

#define write     gen_write
#define set_fs    gen_set_fs
#define close_fs  gen_close_fs

void write(char *msg, ...);
void set_fs(FILE *_fs);
void close_fs();

#endif