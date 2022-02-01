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
static void fprint_type(FILE *fs, TypeDesc *dtype);

void vprint_error(
	int64_t line, char *linep, char *src_end, char *err_pos, char *msg,
	va_list args
) {
	fprintf(stderr, COL_RED "error: " COL_RESET);
	
	while(*msg) {
		if(*msg == '%') {
			msg++;
			if(*msg == 'b') {
				msg++;
				fprintf(stderr, "%02x", va_arg(args, int));
			}
			else if(*msg == 'c') {
				msg++;
				fprintf(stderr, "%c", va_arg(args, int));
			}
			else if(*msg == 's') {
				msg++;
				fprintf(stderr, "%s", va_arg(args, char*));
			}
			else if(*msg == 't') {
				msg++;
				Token *token = va_arg(args, Token*);
				fwrite(token->start, 1, token->length, stderr);
			}
			else if(*msg == 'y') {
				msg++;
				TypeDesc *dtype = va_arg(args, TypeDesc*);
				fprint_type(stderr, dtype);
			}
			else if(*msg == 'u') {
				msg++;
				fprintf(stderr, "%" PRIu64, va_arg(args, uint64_t));
			}
		}
		else {
			fprintf(stderr, "%c", *msg);
			msg++;
		}
	}
	
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

static void fprint_ident(FILE *fs, Token *token)
{
	fprintf(fs, COL_AQUA);
	fwrite(token->start, 1, token->length, fs);
	fprintf(fs, COL_RESET);
}

static void print_ident(Token *token)
{
	fprint_ident(stdout, token);
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

static void fprint_keyword_cstr(FILE *fs, char *str)
{
	fprintf(fs, COL_BLUE "%s" COL_RESET, str);
}

static void print_keyword_cstr(char *str)
{
	fprint_keyword_cstr(stdout, str);
}

static void print_keyword(Token *token)
{
	printf(COL_BLUE);
	fwrite(token->start, 1, token->length, stdout);
	printf(COL_RESET);
}

static void fprint_int(FILE *fs, int64_t val)
{
	fprintf(fs, COL_MAGENTA "%" PRId64 COL_RESET, val);
}

static void print_int(int64_t val)
{
	fprint_int(stdout, val);
}

static void fprint_uint(FILE *fs, uint64_t val)
{
	fprintf(fs, COL_MAGENTA "%" PRIu64 COL_RESET, val);
}

static void print_uint(uint64_t val)
{
	fprint_uint(stdout, val);
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

static void fprint_type(FILE *fs, TypeDesc *dtype)
{
	switch(dtype->type) {
		case TY_NONE:
			fprint_keyword_cstr(fs, "none");
			break;
		case TY_INT64:
			fprint_keyword_cstr(fs, "int64");
			break;
		case TY_UINT8:
			fprint_keyword_cstr(fs, "uint8");
			break;
		case TY_UINT64:
			fprint_keyword_cstr(fs, "uint64");
			break;
		case TY_BOOL:
			fprint_keyword_cstr(fs, "bool");
			break;
		case TY_PTR:
			fprintf(fs, ">");
			fprint_type(fs, dtype->subtype);
			break;
		case TY_ARRAY:
			fprintf(fs, "[");
			if(dtype->length >= 0) fprint_int(fs, dtype->length);
			fprintf(fs, "]");
			fprint_type(fs, dtype->subtype);
			break;
		case TY_FUNC:
			fprint_keyword_cstr(fs, "function");
			break;
		case TY_INST:
			fprint_ident(fs, dtype->id);
			break;
	}
}

static void print_type(TypeDesc *dtype)
{
	fprint_type(stdout, dtype);
}

static void print_expr(Expr *expr)
{
	switch(expr->type) {
		case EX_INT:
			print_int(expr->ival);
			break;
		case EX_BOOL:
			if(expr->ival)
				print_keyword_cstr("true");
			else
				print_keyword_cstr("false");
			break;
		case EX_VAR:
			print_ident(expr->id);
			break;
		case EX_PTR:
			printf("(>");
			print_expr(expr->subexpr);
			printf(")");
			break;
		case EX_DEREF:
			printf("(<");
			print_expr(expr->subexpr);
			printf(")");
			break;
		case EX_CAST:
			printf("(");
			print_expr(expr->subexpr);
			print_keyword_cstr(" as ");
			print_type(expr->dtype);
			printf(")");
			break;
		case EX_SUBSCRIPT:
			printf("(");
			print_expr(expr->subexpr);
			printf("[");
			print_expr(expr->index);
			printf("]");
			printf(")");
			break;
		case EX_BINOP:
			printf("(");
			print_expr(expr->left);
			printf(" %s ", expr->operator->punct);
			print_expr(expr->right);
			printf(")");
			break;
		case EX_ARRAY:
			printf("[");
			for(Expr *item = expr->exprs; item; item = item->next) {
				if(item != expr->exprs)
					printf(", ");
				print_expr(item);
			}
			printf("]");
			break;
		case EX_CALL:
			printf("(");
			print_expr(expr->callee);
			printf("()");
			printf(")");
			break;
		case EX_MEMBER:
			print_expr(expr->subexpr);
			printf(".");
			print_ident(expr->member_id);
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
		case ST_FUNCDECL:
			print_keyword_cstr("function ");
			print_ident(stmt->id);
			printf("()");
			if(stmt->dtype) {
				printf(" : ");
				print_type(stmt->dtype);
			}
			printf(" {\n");
			level ++;
			print_stmts(stmt->func_body);
			level --;
			print_indent();
			printf("}");
			break;
		case ST_STRUCTDECL:
			print_keyword_cstr("struct ");
			print_ident(stmt->id);
			printf(" {\n");
			level ++;
			print_stmts(stmt->struct_body);
			level --;
			print_indent();
			printf("}");
			break;
		case ST_IFSTMT:
			print_keyword_cstr("if ");
			print_expr(stmt->expr);
			printf(" {\n");
			level ++;
			print_stmts(stmt->if_body);
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
		case ST_WHILESTMT:
			print_keyword_cstr("while ");
			print_expr(stmt->expr);
			printf(" {\n");
			level ++;
			print_stmts(stmt->while_body);
			level --;
			print_indent();
			printf("}");
			break;
		case ST_ASSIGN:
			print_expr(stmt->target);
			printf(" = ");
			print_expr(stmt->expr);
			break;
		case ST_CALL:
			print_expr(stmt->call);
			break;
		case ST_RETURN:
			print_keyword_cstr("return ");
			if(stmt->expr)
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

void print_ast(Stmt *stmts)
{
	level = 0;
	printf(COL_YELLOW "=== ast ===" COL_RESET "\n");
	print_stmts(stmts);
}

void print_c_code(char *c_filename)
{
	printf(COL_YELLOW "=== code ===" COL_RESET "\n");
	FILE *fs = fopen(c_filename, "rb");
	
	while(!feof(fs)) {
		int ch = fgetc(fs);
		if(ch == EOF) break;
		fputc(ch, stdout);
	}
	
	printf(COL_YELLOW "=== end ===" COL_RESET "\n");
	fclose(fs);
}
