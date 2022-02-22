#include <stdio.h>
#include <inttypes.h>
#include <ctype.h>
#include <string.h>
#include "print.h"
#include "utils.h"
#include "build.h"

#define COL_RESET   "\x1b[0m"
#define COL_GREY    "\x1b[38;2;170;170;170m"
#define COL_BLUE    "\x1b[38;2;64;128;255m"
#define COL_MAGENTA "\x1b[38;2;255;64;255m"
#define COL_AQUA    "\x1b[38;2;64;255;255m"
#define COL_YELLOW  "\x1b[38;2;255;255;0m"
#define COL_RED     "\x1b[1;31m"

#define COL_YELLOW_BG "\x1b[43m"

static int64_t level;

static void print_stmts(Stmt **stmts);
static void fprint_type(FILE *fs, Type *type);
static void print_block(Block *block);

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
				Type *type = va_arg(args, Type*);
				fprint_type(stderr, type);
			}
			else if(*msg == 'u') {
				msg++;
				fprintf(stderr, "%" PRIu64, va_arg(args, uint64_t));
			}
			else if(*msg == 'i') {
				msg++;
				fprintf(stderr, "%" PRIi64, va_arg(args, int64_t));
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

static void fprint_raw(FILE *fs, char *str)
{
	fwrite(str, 1, strlen(str), fs);
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

static void print_string(char *string, int64_t length)
{
	printf(COL_MAGENTA "\"");
	fwrite(string, 1, length, stdout);
	printf("\"" COL_RESET);
}

static void print_token(Token *token)
{
	switch(token->kind) {
		case TK_IDENT:
			printf("IDENT   ");
			print_ident(token);
			break;
		case TK_INT:
			printf("INT     ");
			print_int(token->ival);
			break;
		case TK_STRING:
			printf("STRING  ");
			print_string(token->string, token->string_length);
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
				fprint_raw(stdout, "PUNCT   " x); \
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
	
	for(Token *token = tokens; token->kind != TK_EOF; token ++) {
		if(last_line != token->line) {
			line_pref_len = print_line_num(
				token->line, array_last(tokens).line
			);
			
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

static void fprint_type(FILE *fs, Type *type)
{
	switch(type->kind) {
		case NONE:
			fprint_keyword_cstr(fs, "none");
			break;
		case INT8:
			fprint_keyword_cstr(fs, "int8");
			break;
		case INT16:
			fprint_keyword_cstr(fs, "int16");
			break;
		case INT32:
			fprint_keyword_cstr(fs, "int32");
			break;
		case INT64:
			fprint_keyword_cstr(fs, "int64");
			break;
		case UINT8:
			fprint_keyword_cstr(fs, "uint8");
			break;
		case UINT16:
			fprint_keyword_cstr(fs, "uint16");
			break;
		case UINT32:
			fprint_keyword_cstr(fs, "uint32");
			break;
		case UINT64:
			fprint_keyword_cstr(fs, "uint64");
			break;
		case BOOL:
			fprint_keyword_cstr(fs, "bool");
			break;
		case STRING:
			fprint_keyword_cstr(fs, "string");
			break;
		case CSTRING:
			fprint_keyword_cstr(fs, "cstring");
			break;
		case PTR:
			if(type->subtype->kind == NONE) {
				fprint_keyword_cstr(fs, "ptr");
			}
			else {
				fprintf(fs, ">");
				fprint_type(fs, type->subtype);
			}
			break;
		case ARRAY:
			fprintf(fs, "[");
			if(type->length >= 0) fprint_int(fs, type->length);
			fprintf(fs, "]");
			fprint_type(fs, type->itemtype);
			break;
		case FUNC:
			fprintf(fs, "(");
			array_for(type->paramtypes, i) {
				if(i > 0) fprintf(fs, ",");
				fprint_type(fs, type->paramtypes[i]);
			}
			fprintf(fs, ")");
			fprint_type(fs, type->returntype);
			break;
		case STRUCT:
			fprint_ident(fs, type->structdecl->id);
			break;
	}
}

static void print_type(Type *type)
{
	fprint_type(stdout, type);
}

static void print_expr(Expr *expr)
{
	switch(expr->kind) {
		case INT:
			print_int(expr->value);
			break;
		case BOOL:
			if(expr->value)
				print_keyword_cstr("true");
			else
				print_keyword_cstr("false");
			break;
		case STRING:
			print_string(expr->string, expr->length);
			break;
		case VAR:
			print_ident(expr->decl->id);
			break;
		case PTR:
			printf("(>");
			print_expr(expr->subexpr);
			printf(")");
			break;
		case DEREF:
			printf("(<");
			print_expr(expr->subexpr);
			printf(")");
			break;
		case CAST:
			printf("(");
			print_expr(expr->subexpr);
			print_keyword_cstr(" as ");
			print_type(expr->type);
			printf(")");
			break;
		case SUBSCRIPT:
			printf("(");
			print_expr(expr->subexpr);
			printf("[");
			print_expr(expr->index);
			printf("]");
			printf(")");
			break;
		case BINOP:
			printf("(");
			print_expr(expr->left);
			printf(" %s ", expr->operator->punct);
			print_expr(expr->right);
			printf(")");
			break;
		case ARRAY:
			printf("[");
			array_for(expr->items, i) {
				if(i > 0) printf(", ");
				print_expr(expr->items	[i]);
			}
			printf("]");
			break;
		case CALL:
			printf("(");
			print_expr(expr->callee);
			printf("(");
			
			array_for(expr->args, i) {
				if(i > 0) printf(", ");
				print_expr(expr->args[i]);
			}
			
			printf("))");
			break;
		case MEMBER:
			print_expr(expr->subexpr);
			printf(".");
			print_ident(expr->member->id);
			break;
	}
}

static void print_vardecl_core(Decl *decl)
{
	print_ident(decl->id);
	printf(" : ");
	print_type(decl->type);
	if(decl->init) {
		printf(" = ");
		print_expr(decl->init);
	}
}

static void print_func(Decl *func)
{
	print_keyword_cstr("function ");
	print_ident(func->id);
	printf("(");
	
	array_for(func->params, i) {
		if(i > 0) printf(", ");
		print_vardecl_core(func->params[i]);
	}
	
	printf(")");
	if(func->type) {
		printf(" : ");
		print_type(func->type->returntype);
	}
	if(!func->isproto) {
		printf(" {\n");
		level ++;
		print_block(func->body);
		level --;
		print_indent();
		printf("}");
	}
}

static void print_stmt(Stmt *stmt)
{
	switch(stmt->kind) {
		case PRINT:
			print_keyword_cstr("print ");
			print_expr(stmt->as_print.expr);
			break;
		case VAR:
			if(stmt->as_decl.exported) print_keyword_cstr("export ");
			print_keyword_cstr("var ");
			print_vardecl_core((Decl*)stmt);
			break;
		case FUNC:
			if(stmt->as_decl.exported) print_keyword_cstr("export ");
			print_func((Decl*)stmt);
			break;
		case STRUCT:
			if(stmt->as_decl.exported) print_keyword_cstr("export ");
			print_keyword_cstr("struct ");
			print_ident(stmt->as_decl.id);
			printf(" {\n");
			level ++;
			print_stmts((Stmt**)stmt->as_decl.members);
			level --;
			print_indent();
			printf("}");
			break;
		case IF:
			print_keyword_cstr("if ");
			print_expr(stmt->as_if.cond);
			printf(" {\n");
			level ++;
			print_stmts(stmt->as_if.if_body->stmts);
			level --;
			print_indent();
			printf("}");
			if(stmt->as_if.else_body) {
				Block *else_body = stmt->as_if.else_body;
				if(
					array_length(else_body->stmts) == 1 &&
					else_body->stmts[0]->kind == IF
				) {
					printf("\n");
					print_indent();
					print_keyword_cstr("else ");
					print_stmt(else_body->stmts[0]);
				}
				else {
					printf("\n");
					print_indent();
					print_keyword_cstr("else");
					printf(" {\n");
					level ++;
					print_stmts(stmt->as_if.else_body->stmts);
					level --;
					print_indent();
					printf("}");
				}
			}
			break;
		case WHILE:
			print_keyword_cstr("while ");
			print_expr(stmt->as_while.cond);
			printf(" {\n");
			level ++;
			print_stmts(stmt->as_while.body->stmts);
			level --;
			print_indent();
			printf("}");
			break;
		case ASSIGN:
			print_expr(stmt->as_assign.target);
			printf(" = ");
			print_expr(stmt->as_assign.expr);
			break;
		case CALL:
			print_expr(stmt->as_call.call);
			break;
		case RETURN:
			print_keyword_cstr("return ");
			if(stmt->as_return.expr)
				print_expr(stmt->as_return.expr);
			break;
		case IMPORT:
			print_keyword_cstr("import ");
			printf(COL_MAGENTA "\"%s\"", stmt->as_import.unit->src_filename);
			break;
	}
}

static void print_stmts(Stmt **stmts)
{
	array_for(stmts, i) {
		print_indent();
		print_stmt(stmts[i]);
		printf("\n");
	}
}

static void print_block(Block *block)
{
	if(block) print_stmts(block->stmts);
}

void print_ast(Block *block)
{
	level = 0;
	printf(COL_YELLOW "=== ast ===" COL_RESET "\n");
	print_block(block);
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
	
	fclose(fs);
}
