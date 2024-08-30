#include "analyze.h"
#include "array.h"
#include "error.h"

static Scope *scope;

static void a_block(Block *block);

static Stmt *lookup(Token *id)
{
	return lookup_in(id, scope);
}

static Expr *get_default_expr(Type *type)
{
	switch(type->kind) {
		case TY_INT8:
		case TY_INT16:
		case TY_INT32:
		case TY_INT64:
		case TY_UINT8:
		case TY_UINT16:
		case TY_UINT32:
		case TY_UINT64:
			return create_int_expr(0);
		case TY_BOOL:
			return create_bool_expr(0);
		case TY_PTR: {
			return create_null_expr();
		}
	}

	return 0;
}

static int types_equal(Type *left, Type *right)
{
	if(left == right)
		return 1;

	if(!left || !right)
		return 0;

	if(left->kind == TY_PTR && right->kind == TY_PTR)
		return types_equal(left->subtype, right->subtype);

	if(left->kind == TY_ARRAY && right->kind == TY_ARRAY)
		return left->length == right->length && types_equal(left->subtype, right->subtype);

	return left->kind == right->kind;
}

static int is_integral_type(Type *type)
{
	return type && type->kind >= TY_INT8 && type->kind <= TY_BOOL;
}

static Expr *adjust_expr_to_type(Expr *expr, Type *type)
{
	if(types_equal(expr->type, type))
		return expr;

	if(is_integral_type(expr->type) && is_integral_type(type))
		return expr;

	if(expr->type->kind == TY_PTR && type->kind == TY_PTR && (expr->type->subtype == 0 || type->subtype == 0))
		return expr;

	error_at(expr, "can not convert type %y to %y", expr->type, type);
	return expr;
}

static void a_expr(Expr *expr)
{
	if(!expr)
		return;

	switch(expr->kind) {
		case EX_INT: case EX_BOOL: case EX_NULL:
			break;

		case EX_VAR: {
			expr->decl = lookup(expr->id);

			if(expr->decl == 0)
				error_at(expr, "name %t is not declared", expr->id);
			else if(expr->decl->end > expr->start)
				error_at(expr, "name %t is declared later", expr->id);
			else
				expr->type = expr->decl->type;
		} break;

		case EX_PTR: {
			a_expr(expr->subexpr);

			if(expr->subexpr)
				expr->type->subtype = expr->subexpr->type;
		} break;

		case EX_DEREF: {
			a_expr(expr->subexpr);

			if(expr->subexpr->type->kind != TY_PTR)
				error_at(expr->subexpr, "expected pointer to dereference");

			expr->type = expr->subexpr->type->subtype;
		} break;

		case EX_SUBSCRIPT: {
			a_expr(expr->subexpr);
			a_expr(expr->index);

			if(expr->subexpr->type->kind != TY_ARRAY)
				error_at(expr->subexpr, "need array to subscript");

			expr->type = expr->subexpr->type->subtype;
		} break;

		default:
			error_at(expr, "INTERNAL: unhandled expression to analyze");
	}
}

static void a_stmt(Stmt *stmt)
{
	if(!stmt)
		return;

	switch(stmt->kind) {

		case ST_VARDECL: {
			a_expr(stmt->init);

			if(!stmt->type)
				stmt->type = stmt->init->type;
			else if(!stmt->init)
				stmt->init = get_default_expr(stmt->type);
			else
				stmt->init = adjust_expr_to_type(stmt->init, stmt->type);
		} break;

		case ST_ASSIGN: {
			a_expr(stmt->target);
			a_expr(stmt->value);

			if(!stmt->target->islvalue)
				error_at(stmt->target, "target is not an l-value");

			stmt->value = adjust_expr_to_type(stmt->value, stmt->target->type);
		} break;

		case ST_PRINT: {
			a_expr(stmt->value);
		} break;

		case ST_IF: {
			a_expr(stmt->cond);
			a_block(stmt->body);

			if(stmt->else_body)
				a_block(stmt->else_body);
		} break;

		case ST_WHILE: {
			a_expr(stmt->cond);
			a_block(stmt->body);
		} break;

		case ST_FUNCDECL: {
			a_block(stmt->body);
		} break;

		default:
			error_at(stmt, "INTERNAL: unknown statement to analyze");
	}
}

static void a_stmts(Stmt **stmts)
{
	array_for(stmts, i)
		a_stmt(stmts[i]);
}

static void a_block(Block *block)
{
	scope = block->scope;
	a_stmts(block->stmts);
	scope = scope->parent;
}

void analyze(Module *module)
{
	a_block(module->root);
}