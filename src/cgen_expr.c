#include "cgen.h"
#include "cgen_utils.h"
#include "utils.h"

static void gen_exprs(Expr **exprs);

static void gen_cast(Expr *expr)
{
	Type *type = expr->type;
	Type *subtype = type->subtype;
	Expr *srcexpr = expr->subexpr;
	Type *srctype = srcexpr->type;
	
	if(type->kind == BOOL) {
		write("(%e ? jatrue : jafalse)", srcexpr);
	}
	else if(is_dynarray_ptr_type(type)) {
		write("((%Y){.length = ", type);
		
		if(srctype->subtype->kind == ARRAY) {
			// from static array
			write("%i", srctype->subtype->length);
		}
		else {
			// other
			write("0");
		}
		
		write(", .items = %e})", srcexpr);
	}
	else if(srctype->kind == STRING && type->kind == CSTRING) {
		if(srcexpr->kind == STRING) {
			write("(\"%S\")", srcexpr->string, srcexpr->length);
		}
		else {
			write("(%e.string)", srcexpr);
		}
	}
	else {
		write("((%Y)%e)", type, srcexpr);
	}
}

static void gen_string(Expr *expr)
{
	write(
		"((jastring){%i, \"%S\"})", expr->length, expr->string, expr->length
	);
}

static void gen_subscript(Expr *expr)
{
	if(
		expr->subexpr->kind == DEREF &&
		is_dynarray_ptr_type(expr->subexpr->subexpr->type)
	) {
		Expr *dynarray = expr->subexpr->subexpr;
		Expr *index = expr->index;
		Type *itemtype = expr->type;
		
		write(
			"(((%y(*)%z)%e.items)[%e])",
			itemtype, itemtype, dynarray, index
		);
	}
	else {
		write("(%e[%e])", expr->subexpr, expr->index);
	}
}

static void gen_binop(Expr *expr)
{
	if(
		expr->left->type->kind == STRING &&
		expr->operator->kind == TK_EQUALS
	) {
		write(
			"(%e.length == %e.length && "
			"memcmp(%e.string, %e.string, %e.length) == 0)",
			expr->left, expr->right,
			expr->left, expr->right, expr->left
		);
	}
	else {
		write("(%e %s %e)", expr->left, expr->operator->punct, expr->right);
	}
}

static void gen_array(Expr *expr)
{
	write("((%Y){", expr->type);
	gen_exprs(expr->items);
	write("})");
}

static void gen_call(Expr *expr)
{
	write("(%e(", expr->callee);
	gen_exprs(expr->args);
	write(")");
	
	if(expr->callee->type->returntype->kind == ARRAY) {
		write(".a");
	}
	
	write(")");
}

static void gen_member(Expr *expr)
{
	write("(%e.%I)", expr->object, expr->member->id);
}

static void gen_exprs(Expr **exprs)
{
	array_for(exprs, i) {
		if(i > 0) write(", ");
		gen_expr(exprs[i]);
	}
}

void gen_init_expr(Expr *expr)
{
	if(expr->kind == ARRAY) {
		write("{");
		
		array_for(expr->items, i) {
			if(i > 0) write(", ");
			gen_init_expr(expr->items[i]);
		}
		
		write("}");
	}
	else if(expr->kind == STRING) {
		write("{%i, \"%S\"}", expr->length, expr->string, expr->length);
	}
	else {
		gen_expr(expr);
	}
}

void gen_length(Expr *expr)
{
	Expr *array = expr->array;
	Type *type = array->type;
	
	if(type->kind == ARRAY) {
		if(type->length == -1) {
			if(array->kind == DEREF) {
				write("(%e.length)", array->ptr);
			}
		}
		else {
			write("%i", type->length);
		}
	}
	else if(type->kind == STRING) {
		write("(%e.length)", array);
	}
}

void gen_new(Expr *expr)
{
	write("(malloc(sizeof(%Y)))", expr->type->subtype);
}

void gen_expr(Expr *expr)
{
	switch(expr->kind) {
		case INT:
			write("%i", expr->value);
			break;
		case BOOL:
			write(expr->value ? "jatrue" : "jafalse");
			break;
		case STRING:
			gen_string(expr);
			break;
		case VAR:
			write("%I", expr->decl->id);
			break;
		case PTR:
			write("(&%e)", expr->subexpr);
			break;
		case DEREF:
			write("(*%e)", expr->ptr);
			break;
		case CAST:
			gen_cast(expr);
			break;
		case SUBSCRIPT:
			gen_subscript(expr);
			break;
		case BINOP:
			gen_binop(expr);
			break;
		case ARRAY:
			gen_array(expr);
			break;
		case CALL:
			gen_call(expr);
			break;
		case MEMBER:
			gen_member(expr);
			break;
		case LENGTH:
			gen_length(expr);
			break;
		case NEW:
			gen_new(expr);
			break;
	}
}
