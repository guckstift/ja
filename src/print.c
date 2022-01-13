#include <stdio.h>
#include <inttypes.h>
#include "print.h"

#define COL_RESET   "\x1b[0m"
#define COL_GREY    "\x1b[38;2;170;170;170m"
#define COL_BLUE    "\x1b[38;2;64;128;255m"
#define COL_MAGENTA "\x1b[38;2;255;64;255m"
#define COL_AQUA    "\x1b[38;2;64;255;255m"
#define COL_YELLOW  "\x1b[38;2;255;255;0m"

void print_ident(Token *token)
{
	printf(COL_AQUA);
	fwrite(token->start, 1, token->length, stdout);
	printf(COL_RESET);
}

void print_keyword_raw(char *start, uint64_t length)
{
	printf(COL_BLUE);
	fwrite(start, 1, length, stdout);
	printf(COL_RESET);
}

void print_keyword_cstr(char *str)
{
	printf(COL_BLUE "%s" COL_RESET, str);
}

void print_keyword(Token *token)
{
	print_keyword_raw(token->start, token->length);
}

void print_int(int64_t val)
{
	printf(COL_MAGENTA "%" PRId64 COL_RESET, val);
}

void print_float(double val)
{
	printf(COL_MAGENTA "%f" COL_RESET, val);
}

void print_token(Token *token)
{
	switch(token->type) {
		case TK_IDENT:
			printf("IDENT   ");
			print_ident(token);
			break;
		case TK_INT:
			printf("INT     ");
			print_int(token->ival);
			break;
		case TK_FLOAT:
			printf("FLOAT   ");
			print_float(token->fval);
			break;
		
		#define F(x) \
			case TK_ ## x: \
				printf("KEYWORD "); \
				print_keyword(token); \
				break;
		
		KEYWORDS(F)
		#undef F
		
		#define F(x, y) \
			case TK_ ## y: \
				printf("PUNCT   " x); \
				break;
		
		PUNCTS(F)
		#undef F
	}
}

void print_tokens(Token *tokens)
{
	int64_t line_pref_len = 0;
	int64_t last_line = 0;
	
	printf(COL_YELLOW "=== tokens ===" COL_RESET "\n");
	
	for(Token *token = tokens; token->type != TK_EOF; token ++) {
		if(last_line != token->line) {
			printf(COL_GREY);
			line_pref_len = printf("%" PRId64 ": ", token->line);
			printf(COL_RESET);
			last_line = token->line;
		}
		else {
			printf("%*c", (int)line_pref_len, ' ');
		}
		print_token(token);
		printf("\n");
	}
}

static void print_expr(Expr *expr)
{
	switch(expr->type) {
		case EX_INT:
			print_int(expr->ival);
			break;
		case EX_VAR:
			print_ident(expr->id);
			break;
	}
}

static void print_exprs(Expr *exprs)
{
	for(Expr *expr = exprs; expr; expr = expr->next) {
		if(expr != exprs) printf(", ");
		print_expr(expr);
	}
}

static void print_stmt(Stmt *stmt)
{
	switch(stmt->type) {
		case ST_PRINT:
			print_keyword_cstr("print ");
			print_exprs(stmt->exprs);
			break;
		case ST_DECL:
			print_keyword_cstr("var ");
			print_ident(stmt->id);
			break;
	}
}

static void print_stmts(Stmt *stmts)
{
	for(Stmt *stmt = stmts; stmt; stmt = stmt->next) {
		print_stmt(stmt);
		printf("\n");
	}
}

void print_unit(Unit *unit)
{
	printf(COL_YELLOW "=== ast ===" COL_RESET "\n");
	print_stmts(unit->stmts);
}

void print_error_line(char *linep, int64_t line, char *pos)
{
}

