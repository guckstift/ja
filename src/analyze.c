#ifndef analyze_H
#define analyze_H

#ifndef IMPLEMENT_FLAG
#define IMPLEMENT_FLAG
#define analyze_C
#endif

#include "ast.c"

void analyze(Module *module);

#endif
#ifdef analyze_C

#include "array.c"
#include "error.c"
#include "table.c"
#include "build.c"

typedef struct {
	Module *module;
	Scope *scope;
	Decl *funchost;
	int in_first_run;
	int should_run_again;
} AnalyzerState;

static AnalyzerState cur_state;

static void a_block(Block *block);

static Decl *lookup(Token *uid)
{
	return lookup_in(uid, cur_state.scope);
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
		case TY_STRING:
			return create_string_expr(0);
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
	if(expr->type->kind == TY_VOID) {
		error_at(expr, "expression has no value");
		return expr;
	}

	if(expr->type->kind == TY_FUNC) {
		error_at(expr, "can not use bare functions as values");
		return expr;
	}

	if(types_equal(expr->type, type))
		return expr;

	if(is_integral_type(expr->type) && is_integral_type(type))
		return expr;

	if(expr->type->kind == TY_PTR && type->kind == TY_PTR && (expr->type->subtype == 0 || type->subtype == 0))
		return expr;

	error_at(expr, "can not convert type %n to %n", expr->type, type);
	return expr;
}

static void a_expr(Expr *expr)
{
	if(!expr)
		return;

	switch(expr->kind) {
		case EX_INT:
		case EX_BOOL:
		case EX_NULL:
		case EX_STRING:
			break;

		case EX_VAR: {
			Decl *decl = expr->decl = lookup(expr->uid);

			if(decl == 0) {
				error_at(expr, "name %n is not declared", expr->uid);
				break;
			}

			if(decl->stmt.kind == ST_VARDECL && decl->stmt.end > expr->start)
				error_at(expr, "variable %n is declared later", expr->uid);

			if(decl->stmt.kind == ST_FUNCDECL)
				expr->islvalue = 0;

			if(decl->stmt.kind == ST_VARDECL && cur_state.funchost) {
				Scope *funcscope = cur_state.funchost->body->scope;
				Scope *varscope = decl->scope;
				// push to deps list of func
				//if(scope_contains_scope(varscope, funcscope)) array_push(funchost->deps, decl);
				if(scope_contains_scope(varscope, funcscope))
					table_add(&cur_state.funchost->deps, decl->uid, decl);
			}

			expr->type = decl->type;

		} break;

		case EX_PTR: {
			a_expr(expr->subexpr);
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

		case EX_CALL: {
			Expr *callee = expr->callee;
			Expr **args = expr->args;
			a_expr(callee);
			Type *functype = callee->type;

			if(functype->kind != TY_FUNC) {
				error_at(callee, "expression is not callable");
				break;
			}

			if(!functype->complete) {
				cur_state.should_run_again = 1;
				break;
			}

			Type **paramtypes = functype->paramtypes;
			int64_t paramcount = array_length(paramtypes);
			int64_t argcount = array_length(expr->args);

			if(argcount < paramcount) {
				error_at(expr, "not enough arguments, %i needed", paramcount);
				break;
			}

			if(argcount > paramcount) {
				error_at(expr, "too many arguments, only %i needed", paramcount);
				break;
			}

			for(int64_t i=0; i < argcount; i++) {
				a_expr(args[i]);
				args[i] = adjust_expr_to_type(args[i], paramtypes[i]);
			}

			expr->type = callee->type->returntype;
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
			Decl *decl = (Decl*)stmt;
			a_expr(decl->init);

			if(!decl->type)
				decl->type = decl->init->type;

			if(!decl->init)
				decl->init = get_default_expr(decl->type);
			else
				decl->init = adjust_expr_to_type(decl->init, decl->type);

			if(decl->type->kind == TY_VOID)
				error_at(stmt, "variable is declared with an empty type");
		} break;

		case ST_ASSIGN: {
			a_expr(stmt->target);
			a_expr(stmt->value);

			if(!stmt->target->islvalue) {
				error_at(stmt->target, "target is not assignable");
				break;
			}

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
			Decl *decl = (Decl*)stmt;
			Decl *old_funchost = decl;
			cur_state.funchost = decl;

			array_for(decl->params, i) {
				Decl *param = decl->params[i];
				a_expr(param->init);
				if(!param->type) param->type = param->init->type;
				if(param->init) param->init = adjust_expr_to_type(param->init, param->type);
				if(param->type->kind == TY_VOID) error_at(param, "parameter is declared with an empty type");
			}

			if(!decl->type->complete) {
				array_for(decl->params, i)
					array_push(decl->type->paramtypes, decl->params[i]->type);

				decl->type->complete = 1;
			}

			a_block(decl->body);
			cur_state.funchost = old_funchost;
		} break;

		case ST_RETURN: {
			Type *returntype = cur_state.funchost->type->returntype;

			if(stmt->value && returntype->kind == TY_VOID) {
				error_at(stmt, "function should not return a value");
				break;
			}

			if(stmt->value == 0 && returntype->kind != TY_VOID) {
				error_at(stmt, "expected expression of type %n to return", returntype);
				break;
			}

			if(stmt->value) {
				a_expr(stmt->value);
				stmt->value = adjust_expr_to_type(stmt->value, returntype);
			}
		} break;

		case ST_CALL: {
			a_expr(stmt->call);
		} break;

		case ST_IMPORT: {
			AnalyzerState old_state = cur_state;
			stmt->module = import_module(stmt->filename, cur_state.module->srcdir);
			cur_state = old_state;
			array_push(cur_state.scope->imports, stmt);
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
	cur_state.scope = block->scope;

	if(cur_state.in_first_run) {
		for(Decl *decl = cur_state.scope->first; decl; decl = decl->next) {
			if(lookup_flat_in(decl->uid, cur_state.scope))
				error_at(decl, "name %n already declared", decl->uid);
			else
				array_push(cur_state.scope->decls, decl);
		}
	}

	a_stmts(block->stmts);
	cur_state.scope = cur_state.scope->parent;
}

void analyze(Module *module)
{
	cur_state.module = module;
	cur_state.scope = 0;
	cur_state.funchost = 0;
	cur_state.in_first_run = 1;

	while(1) {
		cur_state.should_run_again = 0;
		a_block(module->root);
		if(!cur_state.should_run_again) break;
		debug_print(COL_YELLOW "=== analyzing again" COL_RESET "\n");
		cur_state.in_first_run = 0;
	}
}

#endif