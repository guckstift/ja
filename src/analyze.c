#include <stdbool.h>
#include "analyze.h"
#include "array.h"
#include "parse_internal.h"

#include <stdio.h>

static bool repeat_analyze = false;

static void a_block(Block *block);
static void a_expr(Expr *expr);

static Type *a_named_type(Type *type, Token *start, int is_subtype_of_ptr)
{
	Token *id = type->id;
	Decl *decl = lookup(id);
	
	if(!decl)
		fatal_at(start, "type name %t not declared", id);
	
	if(
		decl->kind != STRUCT && decl->kind != ENUM &&
		decl->kind != UNION
	) {
		fatal_at(start, "%t is not a structure, union or enum", id);
	}
	
	if(is_subtype_of_ptr == 0 && decl->end > start) {
		fatal_at(start, "type %t not declared yet", id);
	}
	
	return decl->type;
}

static Type *a_type(Type *type, Token *start, int is_subtype_of_ptr)
{
	switch(type->kind) {
		case NAMED:
			return a_named_type(type, start, is_subtype_of_ptr);
		case PTR:
			type->subtype = a_type(type->subtype, start, 1);
			break;
		case ARRAY:
			type->itemtype = a_type(type->itemtype, start, 0);
			break;
	}
	
	return type;
}

static void eval_integral_cast(Expr *expr, Type *type)
{
	expr->type = type;
	expr->kind = INT;
	
	switch(type->kind) {
		case BOOL:
			expr->kind = BOOL;
			expr->value = expr->value != 0;
			break;
		case INT8:
			expr->value = (int8_t)expr->value;
			break;
		case UINT8:
			expr->value = (uint8_t)expr->value;
			break;
		case INT16:
			expr->value = (int16_t)expr->value;
			break;
		case UINT16:
			expr->value = (uint16_t)expr->value;
			break;
		case INT32:
			expr->value = (int32_t)expr->value;
			break;
		case UINT32:
			expr->value = (uint32_t)expr->value;
			break;
	}
}

static void eval_binop(Expr *expr)
{
	Expr *left = expr->left;
	Expr *right = expr->right;
	
	switch(expr->operator->kind) {
		#define INT_BINOP(name, op) \
			case TK_ ## name: { \
				if(is_integral_type(expr->type)) { \
					expr->kind = INT; \
					expr->value = expr->left->value op \
						expr->right->value; \
				} \
				break; \
			}
		
		#define CMP_BINOP(name, op) \
			case TK_ ## name: { \
				if(is_integral_type(expr->left->type)) { \
					expr->kind = BOOL; \
					expr->value = expr->left->value op \
						expr->right->value; \
				} \
				break; \
			}
		
		INT_BINOP(PLUS, +)
		INT_BINOP(MINUS, -)
		INT_BINOP(MUL, *)
		INT_BINOP(DSLASH, /)
		INT_BINOP(MOD, %)
		INT_BINOP(AMP, &)
		INT_BINOP(PIPE, |)
		INT_BINOP(XOR, ^)
		
		CMP_BINOP(LOWER, <)
		CMP_BINOP(GREATER, >)
		CMP_BINOP(EQUALS, ==)
		CMP_BINOP(NEQUALS, !=)
		CMP_BINOP(LEQUALS, <=)
		CMP_BINOP(GEQUALS, >=)
		
		case TK_AND:
			if(expr->type->kind == STRING) {
				expr->kind = STRING;
				
				if(left->length == 0) {
					expr->string = left->string;
					expr->length = left->length;
				}
				else {
					expr->string = right->string;
					expr->length = right->length;
				}
			}
			else if(is_integral_type(expr->type)) {
				expr->kind = left->kind;
				
				if(left->value == 0) {
					expr->value = left->value;
				}
				else {
					expr->value = right->value;
				}
			}
			
			break;
		
		case TK_OR:
			if(expr->type->kind == STRING) {
				expr->kind = STRING;
				
				if(left->length != 0) {
					expr->string = left->string;
					expr->length = left->length;
				}
				else {
					expr->string = right->string;
					expr->length = right->length;
				}
			}
			else if(is_integral_type(expr->type)) {
				expr->kind = left->kind;
				
				if(left->value != 0) {
					expr->value = left->value;
				}
				else {
					expr->value = right->value;
				}
			}
			
			break;
	}
}

/*
	Might modify expr
*/
static Expr *adjust_expr_to_type(Expr *expr, Type *type, bool explicit)
{
	Type *expr_type = expr->type;
	
	// types equal => no adjustment needed
	if(type_equ(expr_type, type))
		return expr;
	
	// integral types are castable amongst themselves
	if(is_integral_type(expr_type) && is_integral_type(type)) {
		if(expr->isconst) {
			eval_integral_cast(expr, type);
			return expr;
		}
		
		return new_cast_expr(expr, type);
	}
	
	// one pointer to some other by explicit cast always ok
	if(explicit && expr_type->kind == PTR && type->kind == PTR) {
		return new_cast_expr(expr, type);
	}
	
	// array pointer to slice is allowed when itemtypes match
	if(
		type->kind == SLICE && is_array_ptr_type(expr_type) &&
		type_equ(expr_type->subtype->itemtype, type->itemtype)
	) {
		return new_cast_expr(expr, type);
	}
	
	// array literal with matching length
	if(
		expr->kind == ARRAY && type->kind == ARRAY &&
		expr_type->length == type->length
	) {
		array_for(expr->items, i) {
			expr->items[i] = adjust_expr_to_type(
				expr->items[i], type->itemtype, false
			);
		}
		
		expr_type->itemtype = type->itemtype;
		return expr;
	}
	
	fatal_at(
		expr->start,
		"can not convert type  %y  to  %y",
		expr_type, type
	);
}

static void a_var(Expr *expr)
{
	Decl *decl = lookup(expr->id);
	
	if(!decl)
		fatal_at(expr->start, "name %t not declared", expr->id);
	
	if(decl->kind == ENUM) {
		fatal_at(
			expr->start, "%t is an enum that is not declared yet",
			expr->id
		);
	}
	
	if(decl->kind != VAR && decl->kind != FUNC) {
		fatal_at(
			expr->start, "%t is not the name of a variable or function",
			expr->id
		);
	}
	
	if(decl->kind == VAR && decl->end > expr->start)
		fatal_at(expr->start, "variable %t not declared yet", expr->id);
	
	if(decl->kind == VAR && scope->funchost) {
		Decl *func = scope->funchost;
		
		if(func->deps_scanned == 0) {
			Scope *func_scope = func->body->scope;
			Scope *var_scope = decl->scope;
			
			if(scope_contains_scope(var_scope, func_scope)) {
				array_push(func->deps, decl);
			}
		}
	}
	
	if(decl->kind == FUNC) {
		if(decl->deps_scanned == 0) {
			repeat_analyze = true;
		}
		else {
			array_for(decl->deps, i) {
				Decl *dep = decl->deps[i];
				
				if(dep->end > expr->start) {
					fatal_at(
						expr->start, "%t uses %t which is not declared yet",
						expr->id, dep->id
					);
				}
			}
		}
	}
	
	expr->decl = decl;
	expr->type = decl->type;
}

static void a_ptr(Expr *expr)
{
	Expr *subexpr = expr->subexpr;
	a_expr(subexpr);
	expr->type->subtype = subexpr->type;
}

static void a_deref(Expr *expr)
{
	Expr *ptr = expr->ptr;
	a_expr(ptr);
	
	if(ptr->type->kind != PTR)
		fatal_at(ptr->start, "expected pointer to dereference");
	
	expr->type = ptr->type->subtype;
}

static void a_cast(Expr *expr)
{
	a_expr(expr->subexpr);
	*expr = *adjust_expr_to_type(expr->subexpr, expr->type, true);
}

static void a_subscript(Expr *expr)
{
	Expr *array = expr->array;
	Expr *index = expr->index;
	a_expr(array);
	a_expr(index);
	
	while(expr->array->type->kind == PTR) {
		expr->array = new_deref_expr(expr->array->start, expr->array);
		array = expr->array;
	}
	
	if(
		array->type->kind != ARRAY && array->type->kind != SLICE &&
		array->type->kind != STRING
	) {
		fatal_at(array->start, "need array, slice or string to subscript");
	}
	
	if(!is_integral_type(index->type))
		fatal_at(index->start, "index is not an integer or a boolean");
	
	if(
		array->type->kind == ARRAY && array->type->length >= 0 &&
		index->isconst
	) {
		if(index->value < 0 || index->value >= array->type->length)
			fatal_at(
				index->start,
				"index is out of range, must be between 0 .. %u",
				array->type->length - 1
			);
	}
	
	if(array->kind == ARRAY && index->isconst) {
		*expr = *array->items[index->value];
	}
	
	expr->type = array->type->itemtype;
}

static void a_binop(Expr *expr)
{
	Expr *left = expr->left;
	Expr *right = expr->right;
	a_expr(left);
	a_expr(right);
	Type *ltype = left->type;
	Type *rtype = right->type;
	Token *operator = expr->operator;
	OpLevel level = expr->oplevel;
	bool found_match = false;
	bool types_equal = type_equ(ltype, rtype);
	
	if(level == OL_OR || level == OL_AND) {
		if(!types_equal) {
			fatal_at(
				operator, "types must be the same for operator %t", operator
			);
		}
		
		expr->type = ltype;
		found_match = true;
	}
	else if(is_integral_type(ltype) && is_integral_type(rtype)) {
		if(level == OL_CMP) {
			expr->type = new_type(BOOL);
			expr->right = adjust_expr_to_type(right, ltype, false);
			found_match = true;
		}
		else if(level == OL_ADD || level == OL_MUL) {
			expr->type = new_type(INT64);
			expr->left = adjust_expr_to_type(left, expr->type, false);
			expr->right = adjust_expr_to_type(right, expr->type, false);
			found_match = true;
		}
	}
	else if(
		ltype->kind == STRING && rtype->kind == STRING &&
		operator->kind == TK_EQUALS
	) {
		expr->type = new_type(BOOL);
		found_match = true;
	}
	else if(
		ltype->kind == ENUM && rtype->kind == ENUM && types_equal &&
		operator->kind == TK_EQUALS
	) {
		expr->type = new_type(BOOL);
		found_match = true;
	}
	
	if(!found_match) {
		if(types_equal) {
			fatal_at(
				operator, "can not use type  %y  with operator %t",
				ltype, operator
			);
		}
		else {
			fatal_at(
				operator, "can not use types  %y  and  %y  with operator %t",
				ltype, rtype, operator
			);
		}
	}
	
	if(expr->isconst)
		eval_binop(expr);
}

static void a_array(Expr *expr)
{
	Expr **items = expr->items;
	Type *itemtype = 0;
	
	array_for(items, i) {
		a_expr(items[i]);
		
		if(i == 0) {
			itemtype = items[i]->type;
		}
		else {
			items[i] = adjust_expr_to_type(items[i], itemtype, false);
		}
	}
	
	expr->type->itemtype = itemtype;
}

static void a_call(Expr *expr)
{
	Expr *callee = expr->callee;
	Expr **args = expr->args;
	a_expr(callee);
	
	if(callee->type->kind != FUNC)
		fatal_at(callee->start, "expression is not callable");
	
	Type *type = callee->type;
	Type **paramtypes = type->paramtypes;
	
	if(array_length(args) < array_length(paramtypes)) {
		fatal_at(
			expr->start, "not enough arguments, %i needed",
			array_length(paramtypes)
		);
	}
	else if(array_length(args) > array_length(paramtypes)) {
		fatal_at(
			expr->start, "too many arguments, %i needed",
			array_length(paramtypes)
		);
	}
	
	array_for(paramtypes, i) {
		a_expr(args[i]);
		args[i] = adjust_expr_to_type(args[i], paramtypes[i], false);
	}
	
	expr->type = callee->type->returntype;
}

static void a_member(Expr *expr)
{
	Token *member_id = expr->member_id;
	a_expr(expr->object);
	
	while(expr->object->type->kind == PTR) {
		expr->object = new_deref_expr(expr->object->start, expr->object);
	}
	
	Expr *object = expr->object;
	Type *object_type = object->type;
	
	if(
		(object_type->kind == ARRAY || object_type->kind == SLICE ||
			object_type->kind == STRING) &&
		tokequ_str(member_id, "length")
	) {
		*expr = *new_length_expr(object);
	}
	else if(object_type->kind == STRUCT || object_type->kind == UNION) {
		Decl **members = object_type->decl->members;
		Decl *member = 0;
		
		array_for(members, i) {
			if(members[i]->id == member_id) {
				member = members[i];
				break;
			}
		}
		
		if(!member) {
			fatal_at(
				expr->start, "name %t not declared in struct/union", member_id
			);
		}
		
		expr->member = member;
		expr->type = member->type;
	}
	else {
		fatal_at(object->start, "no value to get a member from");
	}
	/*
	else if(object_type->kind == ENUM) {
		Decl *enumdecl = object_type->decl;
		EnumItem **items = enumdecl->items;
		EnumItem *item = 0;
		
		array_for(items, i) {
			if(items[i]->id == member_id) {
				item = items[i];
				break;
			}
		}
		
		if(!item) {
			fatal_at(expr->start, "name %t not declared in enum", member_id);
		}
		
		*expr = *new_enum_item_expr(expr->start, enumdecl, item);
	}
	*/
}

static void a_negation(Expr *expr)
{
	a_expr(expr->subexpr);
	Expr *subexpr = expr->subexpr;
	
	if(!is_integral_type(subexpr->type))
		fatal_at(subexpr->start, "expected integral type to negate");
	
	if(subexpr->kind == NEGATION) {
		*expr = *(subexpr->subexpr);
	}
	else if(expr->isconst) {
		expr->value = -subexpr->value;
		expr->kind = INT;
	}
}

static void a_complement(Expr *expr)
{
	a_expr(expr->subexpr);
	Expr *subexpr = expr->subexpr;
	
	if(!is_integral_type(subexpr->type))
		fatal_at(subexpr->start, "expected integral type to complement");
	
	if(subexpr->kind == COMPLEMENT) {
		*expr = *(subexpr->subexpr);
	}
	else if(expr->isconst) {
		expr->value = ~expr->subexpr->value;
		expr->kind = INT;
	}
}

static void a_expr(Expr *expr)
{
	switch(expr->kind) {
		case VAR:
			a_var(expr);
			break;
		case PTR:
			a_ptr(expr);
			break;
		case DEREF:
			a_deref(expr);
			break;
		case CAST:
			a_cast(expr);
			break;
		case SUBSCRIPT:
			a_subscript(expr);
			break;
		case BINOP:
			a_binop(expr);
			break;
		case ARRAY:
			a_array(expr);
			break;
		case CALL:
			a_call(expr);
			break;
		case MEMBER:
			a_member(expr);
			break;
		case NEGATION:
			a_negation(expr);
			break;
		case COMPLEMENT:
			a_complement(expr);
			break;
	}
}

static void a_print(Print *print)
{
	a_expr(print->expr);
	Expr *expr = print->expr;
	
	if(
		!is_integral_type(expr->type) &&
		expr->type->kind != PTR && expr->type->kind != STRING &&
		expr->type->kind != ARRAY
	) {
		fatal_at(
			expr->start, "can not print value of type  %y", expr->type
		);
	}
}

static void a_vardecl(Decl *decl)
{
	if(decl->init) {
		a_expr(decl->init);
		
		if(decl->init->type->kind == FUNC) {
			fatal_at(
				decl->init->start,
				"can not use a function as value, "
				"make a function pointer with >func_name"
			);
		}
		else if(decl->init->type->kind == NONE)
			fatal_at(decl->init->start, "expression has no value");
		
		if(decl->type == 0)
			decl->type = decl->init->type;
		else
			decl->init = adjust_expr_to_type(decl->init, decl->type, false);
	}
	
	decl->type = a_type(decl->type, decl->start, 0);
}

static void a_funcdecl(Decl *decl)
{
	array_for(decl->params, i) {
		Decl *param = decl->params[i];
		param->type = a_type(param->type, param->start, 0);
	}
	
	decl->type->returntype = a_type(decl->type->returntype, decl->start, 0);
	a_block(decl->body);
	decl->deps_scanned = 1;
}

static void a_structdecl(Decl *decl)
{
	array_for(decl->members, i) {
		Decl *member = decl->members[i];
		a_vardecl(member);
	}
}

static void a_enumdecl(Decl *decl)
{
	int64_t last_val = 0;
	
	array_for(decl->items, i) {
		Expr *val = decl->items[i]->val;
		
		if(val) {
			a_expr(val);
			
			if(!is_integer_type(val->type))
				fatal_at(val->start, "expression must be integer");
			
			last_val = val->value;
		}
		else {
			last_val ++;
			decl->items[i]->val = new_int_expr(0, last_val);
		}
	}
}

static void a_for(For *forstmt)
{
	a_expr(forstmt->from);
	a_expr(forstmt->to);
	
	if(!is_integral_type(forstmt->from->type)) {
		fatal_at(
			forstmt->from->start, "start expression must be of integral type"
		);
	}
	
	if(!is_integral_type(forstmt->to->type)) {
		fatal_at(
			forstmt->to->start, "end expression must be of integral type"
		);
	}
	
	forstmt->iter->type = forstmt->from->type;
	a_block(forstmt->body);
}

static void a_foreach(ForEach *foreach)
{
	a_expr(foreach->array);
	
	while(foreach->array->type->kind == PTR) {
		foreach->array = new_deref_expr(foreach->array->start, foreach->array);
	}
	
	if(foreach->array->type->kind != ARRAY) {
		fatal_at(foreach->array->start, "expected iterable of type array");
	}
	
	foreach->iter->type = foreach->array->type->itemtype;
	a_block(foreach->body);
}

static void a_assign(Assign *assign)
{
	a_expr(assign->target);
	
	if(!assign->target->islvalue)
		fatal_at(assign->target->start, "left side is not assignable");
	
	a_expr(assign->expr);
	
	assign->expr = adjust_expr_to_type(
		assign->expr, assign->target->type, false
	);
}

static void a_return(Return *returnstmt)
{
	if(returnstmt->expr) {
		Decl *funchost = returnstmt->scope->funchost;
		Type *returntype = funchost->type->returntype;
		Expr *returnexpr = returnstmt->expr;
		a_expr(returnexpr);
		returnstmt->expr = adjust_expr_to_type(returnexpr, returntype, false);
	}
}

static void a_stmt(Stmt *stmt)
{
	switch(stmt->kind) {
		case PRINT:
			a_print(&stmt->as_print);
			break;
		case VAR:
			a_vardecl(&stmt->as_decl);
			break;
		case FUNC:
			a_funcdecl(&stmt->as_decl);
			break;
		case STRUCT:
			a_structdecl(&stmt->as_decl);
			break;
		case UNION:
			a_structdecl(&stmt->as_decl);
			break;
		case ENUM:
			a_enumdecl(&stmt->as_decl);
			break;
		case IF:
			a_expr(stmt->as_if.cond);
			a_block(stmt->as_if.if_body);
			if(stmt->as_if.else_body) a_block(stmt->as_if.else_body);
			break;
		case WHILE:
			a_expr(stmt->as_while.cond);
			a_block(stmt->as_while.body);
			break;
		case FOR:
			a_for(&stmt->as_for);
			break;
		case FOREACH:
			a_foreach(&stmt->as_foreach);
			break;
		case ASSIGN:
			a_assign(&stmt->as_assign);
			break;
		case CALL:
			a_call(stmt->as_call.call);
			break;
		case RETURN:
			a_return(&stmt->as_return);
			break;
	}
}

static void a_stmts(Stmt **stmts)
{
	array_for(stmts, i) {
		a_stmt(stmts[i]);
	}
}

static void a_block(Block *block)
{
	scope = block->scope;
	a_stmts(block->stmts);
	scope = scope->parent;
}

void analyze(Unit *unit)
{
	src_end = unit->src + unit->src_len;
	a_block(unit->block);
	
	if(repeat_analyze) {
		repeat_analyze = false;
		
		#ifdef JA_DEBUG
		printf(COL_YELLOW "repeat analyze\n" COL_RESET);
		#endif
		
		analyze(unit);
	}
}
