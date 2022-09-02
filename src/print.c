#include <stdio.h>
#include <inttypes.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include "print.h"
#include "array.h"
#include "build.h"

static int64_t level;

static void print_stmts(Stmt **stmts);
static void fprint_type(FILE *fs, Type *type);
static void print_block(Block *block);
static void fprint_ident(FILE *fs, Token *token);

/*
	format specifiers:
		%b - two digit hex byte
		%c - byte character
		%s - null terminated string
		%t - Token*
		%y - Type*
		%u - unsigned 64 bit integer
		%i - signed 64 bit integer
*/

void ja_vfprintf(FILE *fs, char *msg, va_list args)
{
	while(*msg) {
		if(*msg == '%') {
			msg++;
			if(*msg == 'b') {
				msg++;
				fprintf(fs, "%02x", va_arg(args, int));
			}
			else if(*msg == 'c') {
				msg++;
				fprintf(fs, "%c", va_arg(args, int));
			}
			else if(*msg == 's') {
				msg++;
				fprintf(fs, "%s", va_arg(args, char*));
			}
			else if(*msg == 'p') {
				msg++;
				fprintf(fs, "%p", va_arg(args, void*));
			}
			else if(*msg == 't') {
				msg++;
				Token *token = va_arg(args, Token*);
				
				if(token->kind == TK_IDENT) {
					fprint_ident(fs, token);
				}
				else {
					fwrite(token->start, 1, token->length, fs);
				}
			}
			else if(*msg == 'y') {
				msg++;
				Type *type = va_arg(args, Type*);
				fprint_type(fs, type);
			}
			else if(*msg == 'u') {
				msg++;
				fprintf(fs, "%" PRIu64, va_arg(args, uint64_t));
			}
			else if(*msg == 'i') {
				msg++;
				fprintf(fs, "%" PRIi64, va_arg(args, int64_t));
			}
		}
		else {
			fprintf(fs, "%c", *msg);
			msg++;
		}
	}
}

void ja_fprintf(FILE *fs, char *msg, ...)
{
	va_list args;
	va_start(args, msg);
	ja_vfprintf(fs, msg, args);
	va_end(args);
}

void ja_printf(char *msg, ...)
{
	va_list args;
	va_start(args, msg);
	ja_vfprintf(stdout, msg, args);
	va_end(args);
}

void fprint_marked_src_line(
	FILE *fs, int64_t line, char *linep, char *src_end, char *err_pos
) {
	fprintf(fs, COL_GREY);
	int64_t white_len = fprintf(fs, "%" PRId64 ": ", line);
	fprintf(fs, COL_RESET);
	int64_t col = 0;
	
	for(char *p = linep; p < src_end; p++) {
		if(*p == '\n') {
			break;
		}
		else if(isprint(*p)) {
			fprintf(fs, "%c", *p);
			col ++;
			if(p < err_pos) white_len ++;
		}
		else if(*p == '\t') {
			do {
				fprintf(fs, " ");
				col ++;
				if(p < err_pos) white_len ++;
			} while(col % 4 != 0);
		}
		else {
			fprintf(fs, COL_YELLOW_BG " " COL_RESET);
			col ++;
			if(p < err_pos) white_len ++;
		}
	}
	
	fprintf(fs, "\n");
	for(int64_t i = 0; i < white_len; i++) fprintf(fs, " ");
	fprintf(fs, COL_RED "^" COL_RESET "\n");
}

void vprint_error(
	int64_t line, char *linep, char *src_end, char *err_pos, char *msg,
	va_list args
) {
	fprintf(stderr, COL_RED "error: " COL_RESET);
	ja_vfprintf(stderr, msg, args);
	fprintf(stderr, "\n");
	if(linep == 0) return;
	fprint_marked_src_line(stderr, line, linep, src_end, err_pos);
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
				token->line, array_last(tokens)->line
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
	if(!type) {
		fprintf(fs, "(null)");
		return;
	}
	
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
			fprint_int(fs, type->length);
			fprintf(fs, "]");
			fprint_type(fs, type->itemtype);
			break;
		case SLICE:
			fprintf(fs, "[]");
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
			fprint_ident(fs, type->decl->id);
			break;
		case ENUM:
			fprint_ident(fs, type->decl->id);
			break;
		case UNION:
			fprint_ident(fs, type->decl->id);
			break;
		case NAMED:
			fprint_ident(fs, type->id);
			fprintf(fs, "?");
			break;
	}
}

static void print_type(Type *type)
{
	fprint_type(stdout, type);
}

static void print_expr(Expr *expr)
{
	assert(expr);
	
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
			print_ident(expr->id);
			break;
		case PTR:
			printf(">(");
			print_expr(expr->subexpr);
			printf(")");
			break;
		case DEREF:
			printf("<(");
			print_expr(expr->subexpr);
			printf(")");
			break;
		case CAST:
			printf("(");
			print_expr(expr->subexpr);
			printf(")");
			print_keyword_cstr(" as ");
			print_type(expr->type);
			break;
		case SUBSCRIPT:
			printf("(");
			print_expr(expr->subexpr);
			printf(")[");
			print_expr(expr->index);
			printf("]");
			break;
		case BINOP:
			printf("(");
			print_expr(expr->left);
			printf(") %s (", expr->operator->punct);
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
			printf(")(");
			
			array_for(expr->args, i) {
				if(i > 0) printf(", ");
				print_expr(expr->args[i]);
			}
			
			printf(")");
			break;
		case MEMBER:
			printf("(");
			print_expr(expr->object);
			printf(").");
			print_ident(expr->member_id);
			break;
		case LENGTH:
			printf("(");
			print_expr(expr->array);
			printf(").length");
			break;
		case NEW:
			print_keyword_cstr("new ");
			print_type(expr->type->subtype);
			break;
		case ENUM:
			print_ident(expr->type->decl->id);
			printf(".");
			print_ident(expr->item->id);
			break;
		case NEGATION:
			printf("-(");
			print_expr(expr->subexpr);
			printf(")");
			break;
		case COMPLEMENT:
			printf("~(");
			print_expr(expr->subexpr);
			printf(")");
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
	if(decl->imported) {
		printf(" (imported)");
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
		
		if(array_length(func->deps) > 0) {
			print_indent();
			printf("# uses outer vars: ");
			
			array_for(func->deps, i) {
				print_ident(func->deps[i]->id);
				printf(" ");
			}
			
			printf("\n");
		}
		
		print_block(func->body);
		level --;
		print_indent();
		printf("}");
	}
}

static void print_enumitems(EnumItem **items)
{
	array_for(items, i) {
		print_indent();
		print_ident(items[i]->id);
		printf(" = %li,\n", items[i]->num);
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
		case UNION:
			if(stmt->as_decl.exported) print_keyword_cstr("export ");
			print_keyword_cstr("union ");
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
		case DLLIMPORT:
			print_keyword_cstr("dllimport ");
			printf(
				COL_MAGENTA "\"%s\"" COL_RESET " {\n",
				stmt->as_dll_import.dll_name
			);
			level ++;
			print_stmts((Stmt**)stmt->as_dll_import.decls);
			level --;
			print_indent();
			printf("}");
			break;
		case BREAK:
			print_keyword_cstr("break");
			break;
		case CONTINUE:
			print_keyword_cstr("continue");
			break;
		case FOR:
			print_keyword_cstr("for ");
			print_ident(stmt->as_for.iter->id);
			print_keyword_cstr(" = ");
			print_expr(stmt->as_for.from);
			print_keyword_cstr(" .. ");
			print_expr(stmt->as_for.to);
			printf(" {\n");
			level ++;
			print_stmts(stmt->as_for.body->stmts);
			level --;
			print_indent();
			printf("}");
			break;
		case FOREACH:
			print_keyword_cstr("for ");
			print_ident(stmt->as_foreach.iter->id);
			print_keyword_cstr(" in ");
			print_expr(stmt->as_foreach.array);
			printf(" {\n");
			level ++;
			print_stmts(stmt->as_foreach.body->stmts);
			level --;
			print_indent();
			printf("}");
			break;
		case DELETE:
			print_keyword_cstr("delete ");
			print_expr(stmt->as_delete.expr);
			break;
		case ENUM:
			if(stmt->as_decl.exported) print_keyword_cstr("export ");
			print_keyword_cstr("enum ");
			print_ident(stmt->as_decl.id);
			printf(" {\n");
			level ++;
			print_enumitems(stmt->as_decl.items);
			level --;
			print_indent();
			printf("}");
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
