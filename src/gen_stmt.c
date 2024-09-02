#include "gen_stmt.h"
#include "gen_impl.h"
#include "error.h"
#include "array.h"

static Scope *scope;

static void gen_print(Stmt *print)
{
	Expr *value = print->value;
	Type *type = value->type;
	write("%>printf(\"");

	switch(type->kind) {
		case TY_BOOL:
			write("%%s\", %e ? \"true\" : \"false\"", value);
			break;
		case TY_INT64:
			write("%%\" PRId64, %e", value);
			break;
		case TY_UINT64:
			write("%%\" PRIu64, %e", value);
			break;

		case TY_PTR: {
			if(type->subtype)
				write("<%Y @ %%p>\", %e", type->subtype, value);
			else
				write("<void @ %%p>\", %e", value);
		} break;

		case TY_ARRAY: {
			write("[\");\n");
			Expr *iter = mock_int_var_expr();
			write("%>for(int64_t %j = 0; %j < %i; %j ++) {\n", iter->id, iter->id, type->length, iter->id);
			inclevel();
			Expr *item = create_subscript_expr(value, iter, .type = type->subtype);
			Stmt *itemprint = create_print(item);
			write("%>if(%j > 0) printf(\", \");\n", iter->id);
			gen_print(itemprint);
			declevel();
			write("%>}\n");
			write("%>printf(\"]\"");
		} break;

		default:
			write("%%i\", %e", print->value);
	}

	write(");\n");
}

static void gen_assign(Expr *target, Expr *value)
{
	if(target->type->kind == TY_ARRAY)
		write("%>memcpy(%e, %e, sizeof(%Y));\n", target, value, target->type);
	else
		write("%>%e = %e;\n", target, value);
}

void gen_vardecl(Stmt *decl)
{
	int is_global = !scope;

	write("%>");

	if(is_global)
		write("static ");

	write("%y %j %z", decl->type, decl->id, decl->type);

	if(decl->init && (decl->init->isconst || !is_global))
		write(" = %e", decl->init);

	write(";\n");
}

void gen_stmt(Stmt *stmt)
{
	switch(stmt->kind) {
		case ST_VARDECL: {
			if(scope->parent) {
				// local scope
				gen_vardecl(stmt);
			}
			else if(stmt->init && !stmt->init->isconst) {
				// global scope with non-constant initializer
				Expr *target = create_var_expr(stmt->id, .decl = stmt, .type = stmt->type);
				gen_assign(target, stmt->init);
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
			write("%>if(%e) {\n", stmt->cond);
			gen_block(stmt->body);
			write("%>}\n");

			if(stmt->else_body) {
				write("%>else {\n");
				gen_block(stmt->else_body);
				write("%>}\n");
			}
		} break;

		case ST_WHILE: {
			write("%>while(%e) {\n", stmt->cond);
			gen_block(stmt->body);
			write("%>}\n");
		} break;

		default:
			error_at(stmt, "INTERNAL: unhandled statement to generate");
	}
}

void gen_stmts(Stmt **stmts)
{
	array_for(stmts, i)
		gen_stmt(stmts[i]);
}

void gen_block(Block *block)
{
	scope = block->scope;
	inclevel();
	gen_stmts(block->stmts);
	declevel();
	scope = scope->parent;
}