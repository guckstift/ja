#ifndef error_H
#define error_H

#ifndef IMPLEMENT_FLAG
#define IMPLEMENT_FLAG
#define error_C
#endif

#include "ast.c"
#include "print.c"

#define error_at_tok(t, ...)     (error_tok = t, error(__VA_ARGS__))
#define error_after_tok(t, ...)  (error_after = 1, error_at(t, __VA_ARGS__))

#define error_at(a, ...) \
	_Generic(a, \
		Token*: error_at_tok((Token*)a, __VA_ARGS__), \
		Expr*: error_at_tok(((Expr*)a)->start, __VA_ARGS__), \
		Stmt*: error_at_tok(((Stmt*)a)->start, __VA_ARGS__), \
		Decl*: error_at_tok(((Decl*)a)->stmt.start, __VA_ARGS__) \
	)

#ifdef JA_DEBUG
#define debug_error(...) error(__VA_ARGS__)
#else
#define debug_error(...)
#endif

typedef struct ErrorRecord {
	char *msg;
	int isfatal;
	Token tok;
	int after;
	struct ErrorRecord *next;
} ErrorRecord;

extern Token *error_tok;
extern int    error_after;

extern ErrorRecord *error_first;
extern ErrorRecord *error_last;

void error(char *msg, ...);
void error_reset_opts();
void error_reset_records();
int had_errors();

#endif
#ifdef error_C

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "arena.c"

Token *error_tok = 0; // if set, error() will display the error location in code at this token
int    error_after = 0; // if set, the error location will be at the end of error_tok

ErrorRecord *error_first = 0;
ErrorRecord *error_last = 0;

static int errcount = 0;

static uint64_t print_src_line(Line *line, char *errpos, int grey)
{
	fprintf(stderr, COL_GREY);
	uint64_t offset = fprintf(stderr, "%lu: ", line->index + 1);
	fprintf(stderr, COL_RESET);

	if(grey)
		fprintf(stderr, COL_GREY);

	Token *curtok = line->first;

	for(char *p = line->start; *p && *p != '\n';) {
		if(grey == 0 && curtok && p == curtok->start && curtok->length > 0) {
			fprint(stderr, "%n", curtok);

			if(errpos > p)
				offset += curtok->length;

			p += curtok->length;
			curtok ++;
		}
		else if(*p == '\t') {
			fprint(stderr, "  ");

			if(errpos > p)
				offset += 2;

			p ++;
		}
		else {

			fputc(*p == '\t' ? ' ' : *p, stderr);

			if(errpos > p)
				offset ++;

			p ++;
		}
	}

	if(grey)
		fprintf(stderr, COL_RESET);

	fprintf(stderr, "\n");
	return offset;
}

void error(char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	fprintf(stderr, COL_BOLD COL_RED "error: " COL_RESET);
	vfprint(stderr, msg, args);
	fprintf(stderr, "\n");
	va_end(args);

	if(error_tok) {
		char *errpos = error_tok->start;

		if(error_after)
			errpos += error_tok->length;

		// print previous line(s) if it exists and error_tok is the first token in its own line
		if(error_tok->line && error_tok->line->index > 0 && error_tok->line->first == error_tok) {
			// rewind to the last previous non-empty line
			Line *line = error_tok->line - 1;

			while(line->index > 0 && line->first == 0)
				line --;

			for(;line != error_tok->line; line ++)
				print_src_line(line, errpos, 1);
		}

		uint64_t offset = print_src_line(error_tok->line, errpos, 0);

		for(uint64_t i=0; i < offset; i++)
			fputc(' ', stderr);

		fprintf(stderr, COL_RED "^" COL_RESET "\n");
	}

	#ifdef JA_TEST
		ErrorRecord *record = alloc(sizeof(ErrorRecord));
		record->msg = msg;
		record->isfatal = error_isfatal;
		record->tok = *error_tok;
		record->after = error_after;
		record->next = 0;
		record->arglog = arglog;

		if(error_first)
			error_last = error_last->next = record;
		else
			error_first = error_last = record;
	#endif

	error_reset_opts();
	errcount ++;
}

void error_reset_opts()
{
	error_tok = 0;
	error_after = 0;
}

void error_reset_records()
{
	error_first = 0;
	error_last = 0;
}

int had_errors()
{
	return errcount > 0;
}

#endif