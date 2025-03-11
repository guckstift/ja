#ifndef print_H
#define print_H

#ifndef IMPLEMENT_FLAG
#define IMPLEMENT_FLAG
#define print_C
#endif

#include <stdio.h>
#include <stdarg.h>
#include "ast.c"

#define HEXCHARS "0123456789abcdef"

#define COL_RESET   "\x1b[0m"
#define COL_BOLD    "\x1b[1m"
#define COL_RED     "\x1b[38;2;255;0;0m"
#define COL_MAGENTA "\x1b[38;2;255;64;255m"
#define COL_GREEN   "\x1b[38;2;0;255;0m"
#define COL_DGREEN  "\x1b[38;2;0;128;64m"
#define COL_BLUE    "\x1b[38;2;64;128;255m"
#define COL_YELLOW  "\x1b[38;2;255;255;0m"
#define COL_ORANGE  "\x1b[38;2;255;192;128m"
#define COL_DGREY   "\x1b[38;2;85;85;85m"
#define COL_GREY    "\x1b[38;2;128;128;128m"

#define BGCOL_XDGREY  "\x1b[48;2;32;32;32m"

#define COL_KW(x)      COL_BLUE    x COL_RESET
#define COL_LIT(x)     COL_MAGENTA x COL_RESET
#define COL_IDENT(x)   COL_ORANGE  x COL_RESET
#define COL_COMMENT(x) COL_DGREY   x COL_RESET
#define COL_OK(x)      COL_GREEN   x COL_RESET
#define COL_FAIL(x)    COL_RED     x COL_RESET

#define print(...)              fprint(stdout, __VA_ARGS__)
#define print_tokens(...)       fprint_tokens(stdout, __VA_ARGS__)
#define print_tokens_short(...) fprint_tokens_short(stdout, __VA_ARGS__)
#define print_type(...)         fprint_type(stdout, __VA_ARGS__)
#define print_expr(...)         fprint_expr(stdout, __VA_ARGS__)
#define print_stmt(...)         fprint_stmt(stdout, __VA_ARGS__)
#define print_block(...)        fprint_block(stdout, __VA_ARGS__)
#define print_ast(...)          fprint_ast(stdout, __VA_ARGS__)

#define debug_print(...) fprint(stderr, __VA_ARGS__)

typedef char *(*EscapeMod)(FILE*, char*, va_list);

void inclevel();
void declevel();
void reset_escapemods();
void register_escapemod(char chr, EscapeMod func);
void vfprint(FILE *fs, char *msg, va_list args);
void fprint(FILE *fs, char *msg, ...);
void fprint_tokens(FILE *fs, Token *tokens);
void fprint_tokens_short(FILE *fs, Token *tokens);
void fprint_token(FILE *fs, Token *token);
void fprint_string_literal(FILE *fs, char *strlit, int colored);
void fprint_type(FILE *fs, Type *type);
void fprint_expr(FILE *fs, Expr *expr);
void fprint_stmt(FILE *fs, Stmt *stmt);
void fprint_block(FILE *fs, Block *block);
void fprint_ast(FILE *fs, Block *block);
void print_c_code(char *cfile);

#endif
#ifdef print_C

#include <inttypes.h>
#include "array.c"
#include "table.c"
#include "string.c"

static int level;
static EscapeMod escapemods[256];

void inclevel()
{
	level ++;
}

void declevel()
{
	level --;
}

void reset_escapemods()
{
	for(int i=0; i<256; i++)
		escapemods[i] = 0;
}

void register_escapemod(char chr, EscapeMod func)
{
	uint8_t index = chr;
	escapemods[index] = func;
}

void vfprint(FILE *fs, char *msg, va_list args)
{
	while(*msg) {
		if(*msg == '%') {
			msg ++;

			if(escapemods[*msg] != 0) {
				uint8_t index = *msg;
				msg = escapemods[index](fs, msg, args);
			}
			else if(*msg == '%') {
				fprintf(fs, "%%");
				msg ++;
			}
			else if(*msg == 'i') {
				fprintf(fs, "%" PRIi64, va_arg(args, int64_t));
				msg ++;
			}
			else if(*msg == 'u') {
				fprintf(fs, "%" PRIu64, va_arg(args, uint64_t));
				msg ++;
			}
			else if(*msg == 'c') {
				fprintf(fs, "%c", va_arg(args, int));
				msg ++;
			}
			else if(*msg == 's') {
				fprintf(fs, "%s", va_arg(args, char*));
				msg ++;
			}
			else if(*msg == 'S') {
				fprint_string_literal(fs, va_arg(args, char*), 1);
				msg ++;
			}
			else if(*msg == 'p') {
				fprintf(fs, "%p", va_arg(args, void*));
				msg ++;
			}
			else if(*msg == 'n') {
				void *node = va_arg(args, void*);
				Kind *kind = node;
				Token *token = node;

				if(!node)
					fprintf(fs, COL_FAIL("<null>"));
				else if(*kind > K_STMT)
					fprint_stmt(fs, node);
				else if(*kind > K_EXPR)
					fprint_expr(fs, node);
				else if(*kind > K_TYPE)
					fprint_type(fs, node);
				else
					fprint_token(fs, node);

				msg ++;
			}
			else if(*msg == '>') {
				for(int64_t i = 0; i < level; i ++)
					fprintf(fs, "    ");

				msg ++;
			}
		}
		else if(*msg == '\t') {
			fputc(' ', fs);
			msg ++;
		}
		else {
			fputc(*msg, fs);
			msg ++;
		}
	}
}

void fprint(FILE *fs, char *msg, ...)
{
	va_list args;
	va_start(args, msg);
	vfprint(fs, msg, args);
	va_end(args);
}

void fprint_tokens_short(FILE *fs, Token *tokens)
{
	uint64_t lineindex = -1;

	fprintf(fs, COL_YELLOW "=== tokens ===" COL_RESET "\n");

	array_for(tokens, i) {
		Token *token = tokens + i;

		if(token->kind == TK_EOF)
			break;

		if(lineindex != token->line->index) {
			if(lineindex != -1)
				fprintf(fs, "\n");

			lineindex = token->line->index;
			fprintf(fs, COL_GREY);
			fprintf(fs, "%lu: ", lineindex + 1);
			fprintf(fs, COL_RESET);
		}

		if(token->kind == TK_IDENT)
			//fprint(fs, BGCOL_XDGREY "%n<%p>" COL_RESET " ", token, token->uid);//token->hash);
			fprint(fs, BGCOL_XDGREY "%n" COL_RESET " ", token);
		else
			fprint(fs, BGCOL_XDGREY "%n" COL_RESET " ", token);
	}

	fprintf(fs, "\n");
}

void fprint_token(FILE *fs, Token *token)
{
	char *color =
		token->kind == TK_IDENT ? COL_ORANGE :
		token->kind == TK_INT ? COL_MAGENTA :
		token->kind == TK_STRING ? COL_GREEN :
		token->kind > TK_PUNCTS_START ? "" :
		token->kind > TK_KEYWORDS_START ? COL_BLUE :
		"";
	fprintf(fs, "%s", color);
	fwrite(token->start, 1, token->length, fs);
	fprintf(fs, COL_RESET);
}

void fprint_string_literal(FILE *fs, char *strlit_s, int colored)
{
	uint8_t *strlit = strlit_s;
	if(colored) fprintf(fs, COL_GREEN);
	fprintf(fs, "\"");

	for(int64_t i=0; i < string_length(strlit); i++) {
		if(strlit[i] == '\n') {
			if(colored) fprintf(fs, COL_DGREEN);
			fprintf(fs, "\\n");
			if(colored) fprintf(fs, COL_GREEN);
		}
		else if(strlit[i] == '\t') {
			if(colored) fprintf(fs, COL_DGREEN);
			fprintf(fs, "\\t");
			if(colored) fprintf(fs, COL_GREEN);
		}
		else if(strlit[i] == '"') {
			if(colored) fprintf(fs, COL_DGREEN);
			fprintf(fs, "\\\"");
			if(colored) fprintf(fs, COL_GREEN);
		}
		else if(strlit[i] == '\\') {
			if(colored) fprintf(fs, COL_DGREEN);
			fprintf(fs, "\\\\");
			if(colored) fprintf(fs, COL_GREEN);
		}
		else if(strlit[i] < 0x10 || strlit[i] >= 0x7f) {
			if(colored) fprintf(fs, COL_DGREEN);
			fprintf(fs, "\\x%c%c", HEXCHARS[strlit[i] >> 4], HEXCHARS[strlit[i] & 15]);
			if(colored) fprintf(fs, COL_GREEN);
		}
		else {
			fprintf(fs, "%c", strlit[i]);
		}

		if(colored)
			fprintf(fs, COL_GREEN);
	}

	fprintf(fs, "\"");
	if(colored) fprintf(fs, COL_RESET);
}

void fprint_type(FILE *fs, Type *type)
{
	if(!type) {
		fprint(fs, COL_FAIL("<null-type>"));
		return;
	}

	switch(type->kind) {
		case TY_VOID:
			fprint(fs, COL_KW("void"));
			break;
		case TY_INT8:
			fprint(fs, COL_KW("int8"));
			break;
		case TY_INT16:
			fprint(fs, COL_KW("int16"));
			break;
		case TY_INT32:
			fprint(fs, COL_KW("int32"));
			break;
		case TY_INT64:
			fprint(fs, COL_KW("int64"));
			break;
		case TY_UINT8:
			fprint(fs, COL_KW("uint8"));
			break;
		case TY_UINT16:
			fprint(fs, COL_KW("uint16"));
			break;
		case TY_UINT32:
			fprint(fs, COL_KW("uint32"));
			break;
		case TY_UINT64:
			fprint(fs, COL_KW("uint64"));
			break;
		case TY_BOOL:
			fprint(fs, COL_KW("bool"));
			break;
		case TY_STRING:
			fprint(fs, COL_KW("string"));
			break;

		case TY_PTR: {
			if(type->subtype->kind == TY_VOID)
				fprint(fs, COL_KW("ptr"));
			else
				fprint(fs, "*%n", type->subtype);
		} break;

		case TY_ARRAY:
			fprint(fs, "[" COL_LIT("%i") "]%n", type->length, type->subtype);
			break;

		case TY_FUNC:
			fprint(fs, "(");

			array_for(type->paramtypes, i) {
				if(i > 0) fprint(fs, ",");
				fprint(fs, "%n", type->paramtypes[i]);
			}

			fprint(fs, ")%n", type->returntype);
			break;

		default:
			fprint(fs, COL_RED "<invalid-type>" COL_RESET);
	}
}

void fprint_expr(FILE *fs, Expr *expr)
{
	if(!expr) {
		fprint(fs, COL_FAIL("<null-expr>"));
		return;
	}

	switch(expr->kind) {
		case EX_INT:
			fprint(fs, COL_LIT("%i"), expr->ival);
			break;
		case EX_BOOL:
			fprint(fs, COL_LIT("%s"), expr->bval ? "true" : "false");
			break;
		case EX_NULL:
			fprint(fs, COL_LIT("null"));
			break;
		case EX_STRING:
			fprint(fs, "%S", expr->sval);
			break;
		case EX_VAR:
			fprint(fs, "%n", expr->uid);
			break;
		case EX_PTR:
			fprint(fs, "&(%n)", expr->subexpr);
			break;
		case EX_DEREF:
			fprint(fs, "*(%n)", expr->subexpr);
			break;
		case EX_CAST:
			fprint(fs, "<%n>(%n)", expr->type, expr->subexpr);
			break;
		case EX_SUBSCRIPT:
			fprint(fs, "%n[%n]", expr->subexpr, expr->index);
			break;
		case EX_CALL:
			fprint(fs, "%n()", expr->callee);
			break;
		default:
			fprint(fs, COL_RED "<invalid-expr>" COL_RESET);
	}
}

static void fprint_vardecl_core(FILE *fs, Decl *vardecl)
{
	if(vardecl->uid) fprint(fs, "%n", vardecl->uid);
	if(vardecl->type) fprint(fs, " : %n", vardecl->type);
	if(vardecl->init) fprint(fs, " = %n", vardecl->init);
}

void fprint_stmt(FILE *fs, Stmt *stmt)
{
	if(!stmt) {
		fprint(fs, "%>" COL_FAIL("<null-stmt>"));
		return;
	}

	switch(stmt->kind) {
		case ST_VARDECL:
			Decl *decl = (Decl*)stmt;
			fprint(fs, "%>" COL_KW("var") " ");
			fprint_vardecl_core(fs, decl);
			fprint(fs, ";\n");
			break;

		case ST_ASSIGN:
			fprint(fs, "%>%n = %n;\n", stmt->target, stmt->value);
			break;

		case ST_PRINT:
			fprint(fs, "%>" COL_KW("print") " %n;\n", stmt->value);
			break;

		case ST_IF: {
			fprint(fs, "%>" COL_KW("if") " %n {\n", stmt->cond);
			inclevel();
			fprint_block(fs, stmt->body);
			declevel();
			fprint(fs, "%>}\n");

			if(stmt->else_body) {
				Stmt **else_stmts = stmt->else_body->stmts;

				if(array_length(else_stmts) == 1 && else_stmts[0]->kind == ST_IF) {
					fprint(fs, "%>" COL_KW("else") " ");
					fprint_stmt(fs, else_stmts[0]);
				}
				else {
					fprint(fs, "%>" COL_KW("else") " {\n");
					inclevel();
					fprint_block(fs, stmt->else_body);
					declevel();
					fprint(fs, "%>}\n");
				}
			}
		} break;

		case ST_WHILE:
			fprint(fs, "%>" COL_KW("while") " %n {\n", stmt->cond);
			inclevel();
			fprint_block(fs, stmt->body);
			declevel();
			fprint(fs, "%>}\n");
			break;

		case ST_FUNCDECL: {
			Decl *decl = (Decl*)stmt;
			fprint(fs, "%>" COL_KW("function") " ");
			if(decl->uid) fprint(fs, "%n", decl->uid);
			fprint(fs, "(");

			array_for(decl->params, i) {
				if(i > 0) fprint(fs, ", ");
				fprint_vardecl_core(fs, decl->params[i]);
			}

			fprint(fs, ") ");
			if(decl->type->returntype) fprint(fs, ": %n ", decl->type->returntype);
			fprint(fs, "{\n");
			inclevel();
			fprint(fs, "%>" COL_COMMENT("# deps:"));

			table_for(&decl->deps, i) {
				Token *key = decl->deps.slots[i].key;
				fprint(fs, " %n", key);
			}
			/*
			array_for(decl->deps->slots, i) {
				if(i > 0) fprint(fs, ", ");
				fprint(fs, "%n", decl->deps[i]->uid);
			}
			*/

			fprint(fs, "\n");
			fprint_block(fs, decl->body);
			declevel();
			fprint(fs, "%>}\n");
		} break;

		case ST_RETURN:
			fprint(fs, "%>" COL_KW("return"));
			if(stmt->value) fprint(fs, " %n", stmt->value);
			fprint(fs, ";\n");
			break;

		case ST_CALL:
			fprint(fs, "%>%n;\n", stmt->call);
			break;

		case ST_IMPORT:
			fprint(fs, "%>" COL_KW("import") " %S\n", stmt->filename);
			break;

		default:
			fprint(fs, "%>" COL_RED "<invalid-stmt>\n" COL_RESET);
	}
}

void fprint_stmts(FILE *fs, Stmt **stmts)
{
	array_for(stmts, i)
		fprint_stmt(fs, stmts[i]);
}

void fprint_scope(FILE *fs, Scope *scope)
{
	fprint(fs, "%>" COL_COMMENT("# scope:"));

	for(Decl *decl = scope->first; decl; decl = decl->next) {
		if(decl != scope->first) fprint(fs, ",");
		fprint(fs, " %n:%n", decl->uid, decl->type);
	}

	fprint(fs, "\n");
}

void fprint_block(FILE *fs, Block *block)
{
	fprint_scope(fs, block->scope);
	fprint_stmts(fs, block->stmts);
}

void fprint_ast(FILE *fs, Block *block)
{
	fprintf(fs, COL_YELLOW "=== ast ===" COL_RESET "\n");
	fprint_block(fs, block);
}

void print_c_code(char *cfile)
{
	printf(COL_YELLOW "=== code ===" COL_RESET "\n");
	FILE *fs = fopen(cfile, "r");

	while(!feof(fs)) {
		int ch = fgetc(fs);
		if(ch == EOF) break;
		fputc(ch, stdout);
	}

	fclose(fs);
}

#endif