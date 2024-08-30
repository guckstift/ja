#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "error.h"
#include "print.h"
#include "arena.h"

Token *error_tok = 0; // if set, error() will display the error location in code at this token
int    error_after = 0; // if set, the error location will be at the end of error_tok

ErrorRecord *error_first = 0;
ErrorRecord *error_last = 0;

/*
#define _(x, y) char *ERROR_ ## x = y;
ERRORS
#undef _

#define _(x, y) char *EXPECT_ ## x = y;
EXPECTS
#undef _
*/

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
			fprint(stderr, "%t", curtok);

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

	/*
	if(msg[0] == '%' && msg[1] == '@') {
		error_tok =
			msg[2] == 'T' ? va_arg(args, Token*) :
			msg[2] == 'E' ? va_arg(args, Expr*)->start :
			msg[2] == 'S' ? va_arg(args, Stmt*)->start :
			0;
		msg += 3;
	}
	*/

	fprintf(stderr, COL_BOLD COL_RED "error: " COL_RESET);
	vfprint(stderr, msg, args);
	ArgLog arglog = get_arglog();
	fprintf(stderr, "\n");
	va_end(args);

	if(error_tok) {
		char *errpos = error_tok->start;

		if(error_after)
			errpos += error_tok->length;

		// print previous line if it exists and error_tok is the first token in its own line
		if(error_tok->index > 0 && error_tok[-1].line != error_tok->line)
			print_src_line(error_tok->line - 1, errpos, 1);

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