#include <stdio.h>
#include "cgen_internal.h"
#include "utils/array.h"
#include "utils/string.h"

static Decl *gen_temp_var(Scope *scope, Type *type, Expr *init)
{
	static int64_t counter = 0;
	char buf[256] = {0};
	int64_t len = sprintf(buf, "tmp%lu", counter);
	counter++;
	char *start = malloc(len + 1);
	strcpy(start, buf);
	Token *id = create_id(start, len);
	Decl *decl = new_var(id, scope, id, 0, 0, type, init);
	decl->private_id = 0;
	string_append_token(decl->private_id, id);
	return decl;
}

static void gen_assign(Expr *target, Expr *expr)
{
	if(target->type->kind == ARRAY) {
		if(expr->kind == ARRAY) {
			array_for(expr->items, i) {
				Expr *item = expr->items[i];
				gen_assign(
					new_subscript_expr(target, new_int_expr(target->start, i)),
					item
				);
			}
		}
		else {
			write(
				"%>memcpy(%e, %e, sizeof(%Y));\n",
				target, expr, target->type
			);
		}
	}
	else {
		write("%>%e = %e;\n", target, expr);
	}
}

static void gen_print(Scope *scope, Expr *expr, int repr)
{
	if(expr->type->kind == PTR) {
		write(
			"%>if(%e) {\n"
			INDENT "%>printf(\">\");\n"
			, expr
		);

		inc_level();
		gen_print(scope, new_deref_expr(expr->start, expr), 1);
		dec_level();

		write(
			"%>}\n"
			"%>else {\n"
			INDENT "%>printf(\"null\");\n"
			"%>}\n"
		);
		return;
	}

	if(expr->type->kind == STRING) {
		if(repr) write("%>printf(\"\\\"\");\n");

		if(expr->kind == STRING) {
			write(
				"%>fwrite(\"%S\", 1, %i, stdout);\n",
				expr->string, expr->length, expr->length
			);
		}
		else {
			write(
				"%>fwrite(%e.string, 1, %e.length, stdout);\n",
				expr, expr
			);
		}

		if(repr) write("%>printf(\"\\\"\");\n");
		return;
	}
	else if(expr->type->kind == ARRAY) {
		write("%>printf(\"[\");\n");

		if(expr->kind == ARRAY) {
			array_for(expr->items, i) {
				if(i > 0) write("%>printf(\", \");\n");
				Expr *item = expr->items[i];
				gen_print(scope, item, 1);
			}
		}
		else if(expr->type->length >= 0) {
			Type *type = expr->type;
			Decl *val_tmp = gen_temp_var(scope, type, expr);
			Expr *val_tmp_var = new_var_expr(val_tmp->start, val_tmp);
			write("%>%y %s%z;\n", type, val_tmp->private_id, type);
			gen_assign(val_tmp_var, expr);

			for(int64_t i=0; i < type->length; i++) {
				if(i > 0) write("%>printf(\", \");\n");
				Expr *index = new_int_expr(val_tmp_var->start, i);
				Expr *item = new_subscript_expr(val_tmp_var, index);
				gen_print(scope, item, 1);
			}
		}

		write("%>printf(\"]\");\n");
		return;
	}

	write("%>printf(");

	switch(expr->type->kind) {
		case INT8:
			write("\"%%\" PRId8");
			break;
		case INT16:
			write("\"%%\" PRId16");
			break;
		case INT32:
			write("\"%%\" PRId32");
			break;
		case INT64:
			write("\"%%\" PRId64");
			break;
		case UINT8:
			write("\"%%\" PRIu8");
			break;
		case UINT16:
			write("\"%%\" PRIu16");
			break;
		case UINT32:
			write("\"%%\" PRIu32");
			break;
		case UINT64:
			write("\"%%\" PRIu64");
			break;
		case BOOL:
			write("\"%%s\"");
			break;
		case PTR:
			write("\"%%p\"");
			break;
	}

	write(", ");

	if(expr->type->kind == PTR)
		write("(void*)");

	gen_expr(expr);

	if(expr->type->kind == BOOL)
		write(" ? \"true\" : \"false\"");

	write(");\n");
}

static void gen_if(If *ifstmt)
{
	write("if(%e) {\n", ifstmt->cond);
	gen_block(ifstmt->if_body);
	write("%>}\n");

	if(ifstmt->else_body && ifstmt->else_body->stmts) {
		Stmt **else_body = ifstmt->else_body->stmts;

		if(array_length(else_body) == 1 && else_body[0]->kind == IF) {
			write("%>else ");
			gen_stmt(else_body[0], 1);
		}
		else {
			write("%>else {\n");
			gen_stmts(else_body);
			write("%>}\n");
		}
	}
}

static void gen_return(Return *returnstmt)
{
	if(returnstmt->expr) {
		Expr *result = returnstmt->expr;
		Type *type = result->type;

		if(type->kind == ARRAY) {
			Token *funcid = returnstmt->scope->funchost->id;

			if(result->kind == ARRAY) {
				write("%>return (rt_%I){.a = %E};\n", funcid, result);
			}
			else {
				write("%>{\n");
				inc_level();
				write("%>rt_%I result;\n", funcid);

				write(
					"%>memcpy(&result, %e, sizeof(rt_%I));\n", result, funcid
				);

				write("%>return result;\n");
				dec_level();
				write("%>}\n");
			}
		}
		else {
			write("%>return %e;\n", returnstmt->expr);
		}
	}
	else {
		write("%>return;\n");
	}
}

static void gen_vardecl_stmt(Decl *decl)
{
	if(decl->scope->parent) {
		// local var
		gen_vardecl(decl);
		Expr *init = decl->init;

		if(init && init->isconst == 0 && init->type->kind == ARRAY) {
			gen_assign(new_var_expr(decl->start, decl), init);
		}
	}
	else {
		// global var
		if(decl->init && !decl->init->isconst) {
			// with non-constant initializer
			gen_assign(
				new_var_expr(decl->start, decl),
				decl->init
			);
		}
	}
}

static void gen_for(For *stmt)
{
	Decl *iter = stmt->iter;
	Expr *from = stmt->from;
	Expr *to = stmt->to;
	Type *itertype = iter->type;

	write(
		"%>for("
			"%y %s%z = %e; "
			"%s <= %e; "
			"%s ++) {\n",
		itertype, iter->private_id, itertype, from,
		iter->private_id, to,
		iter->private_id
	);

	gen_block(stmt->body);
	write("%>}\n");
}

static void gen_foreach(ForEach *foreach)
{
	Decl *iter = foreach->iter;
	Expr *array = foreach->array;
	Type *type = array->type;
	Type *itemtype = type->itemtype;

	write("%>for(int64_t it_%t = 0; it_%t < ", iter->id, iter->id);

	if(type->length == -1 && array->kind == DEREF) {
		array = array->ptr;
		write("%e.length; ", array);
		write("it_%t ++) {\n", iter->id);

		write(
			INDENT "%>%y %s%z = ((%y(*)%z)%e.items)[it_%t];\n",
			itemtype, iter->private_id, itemtype,
			itemtype, itemtype, array, iter->id
		);
	}
	else {
		write("%i; ", type->length);
		write("it_%t ++) {\n", iter->id);

		write(
			INDENT "%>%y %s%z = %e[it_%t];\n",
			itemtype, iter->private_id, itemtype, array, iter->id
		);
	}

	gen_block(foreach->body);
	write("%>}\n");
}

static void gen_delete(Delete *stmt)
{
	write("%>free(%e);\n", stmt->expr);
}

void gen_stmt(Stmt *stmt, int noindent)
{
	switch(stmt->kind) {
		case PRINT:
			gen_print(stmt->scope, stmt->as_print.expr, 0);
			write("%>printf(\"\\n\");\n");
			break;
		case VAR:
			gen_vardecl_stmt(&stmt->as_decl);
			break;
		case IF:
			if(noindent == 0) write("%>");
			gen_if(&stmt->as_if);
			break;
		case WHILE:
			write("%>while(%e) {\n", stmt->as_while.cond);
			gen_block(stmt->as_while.body);
			write("%>}\n");
			break;
		case ASSIGN:
			gen_assign(stmt->as_assign.target, stmt->as_assign.expr);
			break;
		case CALL:
			write("%>%e;\n", stmt->as_call.call);
			break;
		case RETURN:
			gen_return(&stmt->as_return);
			break;
		case IMPORT:
			write("%>");
			gen_mainfuncname(stmt->as_import.unit);
			write("(argc, argv);\n");
			break;
		case BREAK:
			write("%>break;\n");
			break;
		case CONTINUE:
			write("%>continue;\n");
			break;
		case FOR:
			gen_for(&stmt->as_for);
			break;
		case FOREACH:
			gen_foreach(&stmt->as_foreach);
			break;
		case DELETE:
			gen_delete(&stmt->as_delete);
			break;
	}
}
