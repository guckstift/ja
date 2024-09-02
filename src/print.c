#include <inttypes.h>
#include "print.h"
#include "array.h"

static int level;
static EscapeMod escapemods[256];
static ArgLog arglog;

ArgLog get_arglog()
{
	return arglog;
}

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
	arglog.argc = 0;

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
			else if(*msg == 's') {
				char *arg = va_arg(args, char*);

				if(arglog.argc < 4) {
					arglog.argv[arglog.argc] = arg;
					arglog.argc ++;
				}

				fprintf(fs, "%s", arg);
				msg ++;
			}
			else if(*msg == 'p') {
				fprintf(fs, "%p", va_arg(args, void*));
				msg ++;
			}
			else if(*msg == 't') {
				Token *token = va_arg(args, Token*);
				char *color =
					token->kind == TK_INT ? COL_MAGENTA :
					token->kind > TK_PUNCTS_START ? "" :
					token->kind > TK_KEYWORDS_START ? COL_BLUE :
					"";
				fprintf(fs, "%s", color);
				fwrite(token->start, 1, token->length, fs);
				fprintf(fs, COL_RESET);
				msg ++;
			}
			else if(*msg == 'y') {
				Type *type = va_arg(args, Type*);
				fprint_type(fs, type);
				msg ++;
			}
			else if(*msg == 'e') {
				Expr *expr = va_arg(args, Expr*);
				fprint_expr(fs, expr);
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

static void fprint_token(FILE *fs, Token *token)
{
	if(token->kind == TK_EOF)
		fprintf(fs, "EOF     ");
	else if(token->kind == TK_INT)
		fprintf(fs, "INT     ");
	else if(token->kind == TK_IDENT)
		fprintf(fs, "IDENT   ");
	else if(token->kind > TK_PUNCTS_START)
		fprintf(fs, "PUNCT   ");
	else if(token->kind > TK_KEYWORDS_START)
		fprintf(fs, "KEYWORD ");

	fprint(fs, "%t\n", token);
}

static void fprint_token_short(FILE *fs, Token *token)
{
	fprint(fs, BGCOL_XDGREY "%t" COL_RESET " ", token);
}

void fprint_tokens(FILE *fs, Token *tokens)
{
	uint64_t lineindex = -1;
	uint64_t space = 0;

	fprintf(fs, COL_YELLOW "=== tokens ===" COL_RESET "\n");

	array_for(tokens, i) {
		Token *token = tokens + i;

		if(lineindex != token->line->index) {
			lineindex = token->line->index;
			fprintf(fs, COL_GREY);
			space = fprintf(fs, "%lu: ", lineindex + 1);
			fprintf(fs, COL_RESET);
		}
		else {
			for(uint64_t i=0; i<space; i++)
				fprintf(fs, " ");
		}

		fprint_token(fs, token);
	}
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

		fprint_token_short(fs, token);
	}

	fprintf(fs, "\n");
}

void fprint_type(FILE *fs, Type *type)
{
	if(!type) {
		fprint(fs, COL_FAIL("<null-type>"));
		return;
	}

	switch(type->kind) {
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

		case TY_PTR: {
			if(type->subtype)
				fprint(fs, "*%y", type->subtype);
			else
				fprint(fs, COL_KW("ptr"));
		} break;

		case TY_ARRAY:
			fprint(fs, "[" COL_LIT("%i") "]%y", type->length, type->subtype);
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
			fprint(fs, COL_KW("null"));
			break;
		case EX_VAR:
			fprint(fs, "%t", expr->id);
			break;
		case EX_PTR:
			fprint(fs, "&(%e)", expr->subexpr);
			break;
		case EX_DEREF:
			fprint(fs, "*(%e)", expr->subexpr);
			break;
		case EX_CAST:
			fprint(fs, "<%y>(%e)", expr->type, expr->subexpr);
			break;
		case EX_SUBSCRIPT:
			fprint(fs, "%e[%e]", expr->subexpr, expr->index);
			break;
		default:
			fprint(fs, COL_RED "<invalid-expr>" COL_RESET);
	}
}

void fprint_stmt(FILE *fs, Stmt *stmt)
{
	if(!stmt) {
		fprint(fs, "%>" COL_FAIL("<null-stmt>"));
		return;
	}

	switch(stmt->kind) {
		case ST_VARDECL:
			fprint(fs, "%>" COL_KW("var") " ");
			if(stmt->id) fprint(fs, "%t", stmt->id);
			if(stmt->type) fprint(fs, " : %y", stmt->type);
			if(stmt->init) fprint(fs, " = %e", stmt->init);
			fprint(fs, ";\n");
			break;

		case ST_ASSIGN:
			fprint(fs, "%>" "%e = %e;\n", stmt->target, stmt->value);
			break;

		case ST_PRINT:
			fprint(fs, "%>" COL_KW("print") " %e;\n", stmt->value);
			break;

		case ST_IF: {
			fprint(fs, "%>" COL_KW("if") " %e {\n", stmt->cond);
			inclevel();
			fprint_block(fs, stmt->body);
			declevel();
			fprint(fs, "%>" "}\n");

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
					fprint(fs, "%>" "}\n");
				}
			}
		} break;

		case ST_WHILE:
			fprint(fs, "%>" COL_KW("while") " %e {\n", stmt->cond);
			inclevel();
			fprint_block(fs, stmt->body);
			declevel();
			fprint(fs, "%>" "}\n");
			break;

		case ST_FUNCDECL:
			fprint(fs, "%>" COL_KW("function") " ");
			if(stmt->id) fprint(fs, "%t", stmt->id);
			fprint(fs, "() {\n");
			inclevel();
			fprint_block(fs, stmt->body);
			declevel();
			fprint(fs, "%>" "}\n");
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
	fprint(fs, "%># scope: ");

	array_for(scope->decls, i) {
		Stmt *decl = scope->decls[i];
		fprint(fs, "%s%t", i > 0 ? ", " : "", decl->id);
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
		if(ch == EOF)
			break;
		fputc(ch, stdout);
	}

	fclose(fs);
}