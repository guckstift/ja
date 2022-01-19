#include <stdio.h>
#include <inttypes.h>
#include <ctype.h>
#include "print.h"

#define COL_RESET   "\x1b[0m"
#define COL_GREY    "\x1b[38;2;170;170;170m"
#define COL_BLUE    "\x1b[38;2;64;128;255m"
#define COL_MAGENTA "\x1b[38;2;255;64;255m"
#define COL_AQUA    "\x1b[38;2;64;255;255m"
#define COL_YELLOW  "\x1b[38;2;255;255;0m"
#define COL_RED     "\x1b[1;31m"

#define COL_YELLOW_BG "\x1b[43m"

static int64_t level;

static void print_stmts(Stmt *stmts);

void vprint_error(
	int64_t line, char *linep, char *src_end, char *err_pos, char *msg,
	va_list args
) {
	fprintf(stderr, COL_RED "error: " COL_RESET);
	vfprintf(stderr, msg, args);
	fprintf(stderr, "\n");
	
	if(linep == 0) return;
	
	fprintf(stderr, COL_GREY);
	int64_t white_len = fprintf(stderr, "%" PRId64 ": ", line);
	fprintf(stderr, COL_RESET);
	int64_t col = 0;
	
	for(char *p = linep; p < src_end; p++) {
		if(*p == '\n') {
			break;
		}
		else if(isprint(*p)) {
			fprintf(stderr, "%c", *p);
			col ++;
			if(p < err_pos) white_len ++;
		}
		else if(*p == '\t') {
			do {
				fprintf(stderr, " ");
				col ++;
				if(p < err_pos) white_len ++;
			} while(col % 4 != 0);
		}
		else {
			fprintf(stderr, COL_YELLOW_BG " " COL_RESET);
			col ++;
			if(p < err_pos) white_len ++;
		}
	}
	
	fprintf(stderr, "\n");
	for(int64_t i = 0; i < white_len; i++) fprintf(stderr, " ");
	fprintf(stderr, COL_RED "^" COL_RESET);
	fprintf(stderr, "\n");
}

void print_error(
	int64_t line, char *linep, char *src_end, char *err_pos, char *msg, ...
) {
	va_list args;
	va_start(args, msg);
	vprint_error(line, linep, src_end, err_pos, msg, args);
	va_end(args);
}

static void print_ident(Token *token)
{
	printf(COL_AQUA);
	fwrite(token->start, 1, token->length, stdout);
	printf(COL_RESET);
}

static int64_t dec_len(int64_t val)
{
	int64_t count = 1;
	while(val >= 10) {
		val /= 10;
		count ++;
	}
	return count;
}

static int64_t print_line_num(int64_t line, int64_t max_line)
{
	int64_t line_dec_len = dec_len(line);
	int64_t max_line_dec_len = dec_len(max_line);
	int64_t printf_len = 0;
	printf(COL_GREY);
	for(int64_t i = line_dec_len; i < max_line_dec_len; i++)
		printf_len += printf(" ");
	printf_len += printf("%" PRId64 ": ", line);
	printf(COL_RESET);
	return printf_len;
}

static void print_keyword_cstr(char *str)
{
	printf(COL_BLUE "%s" COL_RESET, str);
}

static void print_keyword(Token *token)
{
	printf(COL_BLUE);
	fwrite(token->start, 1, token->length, stdout);
	printf(COL_RESET);
}

static void print_int(int64_t val)
{
	printf(COL_MAGENTA "%" PRId64 COL_RESET, val);
}

static void print_float(double val)
{
	printf(COL_MAGENTA "%f" COL_RESET, val);
}

static void print_token(Token *token)
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

void print_tokens(Tokens *tokens)
{
	int64_t line_pref_len = 0;
	int64_t last_line = 0;
	
	printf(COL_YELLOW "=== tokens ===" COL_RESET "\n");
	
	for(Token *token = tokens->first; token->type != TK_EOF; token ++) {
		if(last_line != token->line) {
			line_pref_len = print_line_num(token->line, tokens->last[-1].line);
			last_line = token->line;
		}
		else {
			printf("%*c", (int)line_pref_len, ' ');
		}
		print_token(token);
		printf("\n");
	}
}

static void print_indent()
{
	for(int64_t i=0; i<level; i++) printf("  ");
}

static void print_type(TypeDesc *dtype)
{
	switch(dtype->type) {
		case TY_INT64:
			print_keyword_cstr("int64");
			break;
		case TY_UINT64:
			print_keyword_cstr("uint64");
			break;
		case TY_BOOL:
			print_keyword_cstr("bool");
			break;
		case TY_PTR:
			printf(">");
			print_type(dtype->subtype);
			break;
	}
}

static void print_expr(Expr *expr)
{
	switch(expr->type) {
		case EX_INT:
			print_int(expr->ival);
			break;
		case EX_BOOL:
			if(expr->bval)
				print_keyword_cstr("true");
			else
				print_keyword_cstr("false");
			break;
		case EX_VAR:
			print_ident(expr->id);
			break;
		case EX_PTR:
			printf(">");
			print_expr(expr->expr);
			break;
		case EX_DEREF:
			printf("<");
			print_expr(expr->expr);
			break;
		case EX_CAST:
			print_expr(expr->expr);
			print_keyword_cstr(" as ");
			print_type(expr->dtype);
			break;
		case EX_BINOP:
			print_expr(expr->left);
			printf(" + ");
			print_expr(expr->right);
			break;
	}
}

static void print_stmt(Stmt *stmt)
{
	switch(stmt->type) {
		case ST_PRINT:
			print_keyword_cstr("print ");
			print_expr(stmt->expr);
			break;
		case ST_VARDECL:
			print_keyword_cstr("var ");
			print_ident(stmt->id);
			printf(" : ");
			print_type(stmt->dtype);
			if(stmt->expr) {
				printf(" = ");
				print_expr(stmt->expr);
			}
			break;
		case ST_IFSTMT:
			print_keyword_cstr("if ");
			print_expr(stmt->expr);
			printf(" {\n");
			level ++;
			print_stmts(stmt->body);
			level --;
			print_indent();
			printf("}");
			if(stmt->else_body) {
				printf("\n");
				print_indent();
				print_keyword_cstr("else");
				printf(" {\n");
				level ++;
				print_stmts(stmt->else_body);
				level --;
				print_indent();
				printf("}");
			}
			break;
		case ST_ASSIGN:
			print_expr(stmt->target);
			printf(" = ");
			print_expr(stmt->expr);
			break;
	}
}

static void print_stmts(Stmt *stmts)
{
	for(Stmt *stmt = stmts; stmt; stmt = stmt->next) {
		print_indent();
		print_stmt(stmt);
		printf("\n");
	}
}

void print_unit(Unit *unit)
{
	level = 0;
	printf(COL_YELLOW "=== ast ===" COL_RESET "\n");
	print_stmts(unit->stmts);
}
