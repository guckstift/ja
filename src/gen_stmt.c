#ifndef gen_stmt_H
#define gen_stmt_H

#ifndef IMPLEMENT_FLAG
#define IMPLEMENT_FLAG
#define gen_stmt_C
#endif

#include "ast.c"

void gen_vardecl(Decl *decl);
void gen_block(Block *block);
void gen_funcdecl(Decl *decl);

#endif
#ifdef gen_stmt_C

#define IMPLEMENT_FLAG

#include <string.h>
#include "gen_impl.c"
#include "error.c"
#include "array.c"
#include "arena.c"

static Scope *scope;

static Token *mock_id()
{
	char buf[32];
	sprintf(buf, "%lu", scope->mock_id_counter);
	scope->mock_id_counter ++;
	int64_t len = strlen(buf);
	char *str = alloc(len + 1);
	strcpy(str, buf);
	Token *token = create_token(TK_IDENT, .start = str, .length = len);
	token->uid = token;
	return token;
}

static Expr *mock_int_var_expr()
{
	Type *type = create_type(TY_INT);
	Token *id = mock_id();
	Decl *decl = create_vardecl(id, create_int_expr(0), type);
	Expr *var = create_var_expr(id, .decl = decl);
	return var;
}

static void gen_print(Stmt *print)
{
	Expr *value = print->value;
	Type *type = value->type;

	switch(type->kind) {
		case TY_BOOL:
			write("%>print_bool(%e);\n", value);
			break;
		case TY_INT8:
		case TY_INT16:
		case TY_INT32:
		case TY_INT64:
			write("%>print_int(%e);\n", value);
			break;
		case TY_UINT8:
		case TY_UINT16:
		case TY_UINT32:
		case TY_UINT64:
			write("%>print_uint(%e);\n", value);
			break;
		case TY_STRING:
			write("%>print_string(%e);\n", value);
			break;

		case TY_PTR: {
			if(value->kind == EX_NULL)
				write("%>printf(\"null\");\n");
			else if(type->subtype)
				write("%>printf(\"<%Y @ %%p>\", %e);\n", type->subtype, value);
			else
				write("%>printf(\"<void @ %%p>\", %e);\n", value);
		} break;

		case TY_ARRAY: {
			Expr *iter = mock_int_var_expr();
			Expr *item = create_subscript_expr(value, iter, .type = type->subtype);
			Stmt *itemprint = create_print(item);
			write("%>printf(\"[\");\n");
			write("%>for(int64_t %e = 0; %e < %i; %e ++) {\n", iter, iter, type->length, iter);
			inclevel();
			write("%>if(%e > 0) printf(\", \");\n", iter);
			gen_print(itemprint);
			declevel();
			write("%>}\n");
			write("%>printf(\"]\");\n");
		} break;

		default:
			error_at(print, "INTERNAL: unhandled expression type to generate print statement for");
	}
}

static void gen_assign(Expr *target, Expr *value)
{
	if(target->type->kind == TY_ARRAY)
		write("%>memcpy(%e, %e, sizeof(%Y));\n", target, value, target->type);
	else
		write("%>%e = %e;\n", target, value);
}

void gen_vardecl(Decl *decl)
{
	int is_global = !scope;

	write("%>");

	if(is_global)
		write("static ");

	write("%y %j%z", decl->type, decl->uid, decl->type);

	if(decl->init && (decl->init->isconst || !is_global))
		write(" = %E", decl->init);

	write(";\n");
}

static void gen_stmt(Stmt *stmt, int no_indent_if_stmt)
{
	switch(stmt->kind) {
		case ST_VARDECL: {
			Decl *decl = (Decl*)stmt;

			if(scope->parent) {
				// local scope
				gen_vardecl(decl);
			}
			else if(decl->init && !decl->init->isconst) {
				// global scope with non-constant initializer
				Expr *target = create_var_expr(decl->uid, .decl = decl, .type = decl->type);
				gen_assign(target, decl->init);
			}
		} break;

		case ST_FUNCDECL: {
			// ignore
		} break;

		case ST_ASSIGN: {
			gen_assign(stmt->target, stmt->value);
		} break;

		case ST_PRINT: {
			gen_print(stmt);
			write("%>printf(\"\\n\");\n");
		} break;

		case ST_IF: {
			if(!no_indent_if_stmt) write("%>");
			write("if(%e) {\n", stmt->cond);
			gen_block(stmt->body);
			write("%>}\n");

			if(stmt->else_body) {
				Stmt **else_stmts = stmt->else_body->stmts;

				if(array_length(else_stmts) == 1 && else_stmts[0]->kind == ST_IF) {
					write("%>else ");
					gen_stmt(else_stmts[0], 1);
				}
				else {
					write("%>else {\n");
					gen_block(stmt->else_body);
					write("%>}\n");
				}
			}
		} break;

		case ST_WHILE: {
			write("%>while(%e) {\n", stmt->cond);
			gen_block(stmt->body);
			write("%>}\n");
		} break;

		case ST_RETURN: {
			write("%>return");
			if(stmt->value) write(" %e", stmt->value);
			write(";\n");
		} break;

		case ST_CALL: {
			write("%>%e;\n", stmt->call);
		} break;

		case ST_IMPORT: {
			write("%>main_%s(argc, argv);\n", stmt->module->uid);
		} break;

		default:
			error_at(stmt, "INTERNAL: unhandled statement to generate");
	}
}

static void gen_stmts(Stmt **stmts)
{
	array_for(stmts, i)
		gen_stmt(stmts[i], 0);
}

void gen_block(Block *block)
{
	scope = block->scope;
	inclevel();
	gen_stmts(block->stmts);
	declevel();
	scope = scope->parent;
}

void gen_funcdecl(Decl *decl)
{
	write("%y %j(", decl->type->returntype, decl->uid);

	array_for(decl->params, i) {
		Decl *param = decl->params[i];
		if(i > 0) write(", ");
		write("%y %j%z", param->type, param->uid, param->type);
	}

	write(")%z {\n", decl->type->returntype);
	gen_block(decl->body);
	write("}\n");
}

#endif