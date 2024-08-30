#ifndef GEN_EXPR_H
#define GEN_EXPR_H

#include <stdarg.h>
#include <stdio.h>

char *write_expr(FILE *fs, char *msg, va_list args);

#endif