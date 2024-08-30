#ifndef GEN_TYPE_H
#define GEN_TYPE_H

#include <stdarg.h>
#include <stdio.h>

char *write_type(FILE *fs, char *msg, va_list args);
char *write_type_postfix(FILE *fs, char *msg, va_list args);
char *write_full_type(FILE *fs, char *msg, va_list args);

#endif