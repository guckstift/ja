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
	else if(expr->subexpr->type->kind == STRING) {
		Expr *string = expr->subexpr;
		Expr *index = expr->index;
		
		write(
			"((jastring){1L, %e.string + %e})",
			string, index
		);
	}
	else {
		write("(%e[%e])", expr->subexpr, expr->index);
	}
}

static void gen_binop(Expr *expr)
{
	Expr *left = expr->left;
	Expr *right = expr->right;
	Token *op = expr->operator;
	
	if(left->type->kind == STRING && op->kind == TK_EQUALS) {
		write(
			"(%e.length == %e.length && "
			"memcmp(%e.string, %e.string, %e.length) == 0)",
			expr->left, expr->right,
			expr->left, expr->right, expr->left
		);
	}
	else if(op->kind == TK_AND) {
		if(expr->type->kind == STRING) {
			write("(%e.length == 0 ? %e : %e)", left, left, right);
		}
		else {
			write("(!(%e) ? %e : %e)", left, left, right);
		}
	}
	else if(op->kind == TK_OR) {
		if(expr->type->kind == STRING) {
			write("(%e.length != 0 ? %e : %e)", left, right, left);
		}
		else {
			write("(%e ? %e : %e)", left, right, left);
		}
	}
	else {
		write("(%e %s %e)", expr->left, expr->operator->punct, expr->right);
	}
}

static void gen_exprs(Expr **exprs)
{
	array_for(exprs, i) {
		if(i > 0) write(", ");
		gen_expr(exprs[i]);
	}
}

static void gen_args(Expr **exprs)
{
	array_for(exprs, i) {
		if(i > 0) write(", ");
		Expr *expr = exprs[i];
		Type *type = expr->type;
		
		if(type->kind == ARRAY) {
			if(expr->kind == ARRAY) {
				array_for(expr->items, j) {
					Expr *item = expr->items[j];
					if(j > 0) write(", ");
					write("%e", item);
				}
			}
			else {
				for(int64_t j=0; j < type->length; j++) {
					if(j > 0) write(", ");
					write("%e[%i]", expr, j);
				}
			}
		}
		else {
			gen_expr(exprs[i]);
		}
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
	gen_args(expr->args);
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

void gen_enum_item(Expr *expr)
{
	EnumItem *item = expr->item;
	Type *type = expr->type;
	Decl *decl = type->decl;
	
	if(decl->exported) {
		write(
			"_%s_ja_%t",
			decl->public_id, item->id
		);
	}
	else {
		write(
			"_%s_ja_%t",
			decl->private_id, item->id
		);
	}
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
		case ENUM:
			gen_enum_item(expr);
			break;
		case NEGATION:
			write("(-%e)", expr->subexpr);
			break;
		case COMPLEMENT:
			write("(~%e)", expr->subexpr);
			break;
	}
}
