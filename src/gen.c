#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include "gen.h"

#define INDENT      "    "
#define COL_YELLOW  "\x1b[38;2;255;255;0m"
#define COL_RESET   "\x1b[0m"

static FILE *ofs;
static int64_t level;

static void gen_stmts(Stmt *stmts);
static void gen_vardecls(Stmt *stmts);
static void gen_vardecl(Stmt *stmt);
static void gen_assign(Expr *target, Expr *expr);
static void gen_type(TypeDesc *dtype);
static void gen_type_postfix(TypeDesc *dtype);
static void gen_expr(Expr *expr);
static void gen_init_expr(Expr *expr);

static void write(char *msg, ...)
{
	va_list args;
	va_start(args, msg);
	
	while(*msg) {
		if(*msg == '%') {
			msg++;
			if(*msg == '%') {
				msg++;
				fputc('%', ofs);
			}
			else if(*msg == '>') {
				msg++;
				for(int64_t i=0; i<level; i++) write(INDENT);
			}
			else if(*msg == 'c') {
				msg++;
				fprintf(ofs, "%c", va_arg(args, int));
			}
			else if(*msg == 's') {
				msg++;
				fprintf(ofs, "%s", va_arg(args, char*));
			}
			else if(*msg == 't') {
				msg++;
				Token *token = va_arg(args, Token*);
				fwrite(token->start, 1, token->length, ofs);
			}
			else if(*msg == 'y') {
				msg++;
				TypeDesc *dtype = va_arg(args, TypeDesc*);
				gen_type(dtype);
			}
			else if(*msg == 'z') {
				msg++;
				TypeDesc *dtype = va_arg(args, TypeDesc*);
				gen_type_postfix(dtype);
			}
			else if(*msg == 'Y') {
				msg++;
				TypeDesc *dtype = va_arg(args, TypeDesc*);
				gen_type(dtype);
				gen_type_postfix(dtype);
			}
			else if(*msg == 'e') {
				msg++;
				Expr *expr = va_arg(args, Expr*);
				gen_expr(expr);
			}
			else if(*msg == 'E') {
				msg++;
				Expr *expr = va_arg(args, Expr*);
				gen_init_expr(expr);
			}
			else if(*msg == 'I') {
				msg++;
				write("ja_%t", va_arg(args, Token*));
			}
			else if(*msg == 'i') {
				msg++;
				fprintf(ofs, "%" PRId64 "L", va_arg(args, int64_t));
			}
			else if(*msg == 'u') {
				msg++;
				fprintf(ofs, "%" PRIu64 "UL", va_arg(args, uint64_t));
			}
		}
		else {
			fputc(*msg, ofs);
			msg++;
		}
	}
	
	va_end(args);
}

static void gen_type(TypeDesc *dtype)
{
	switch(dtype->type) {
		case TY_NONE:
			write("void");
			break;
		case TY_INT64:
			write("int64_t");
			break;
		case TY_UINT8:
			write("uint8_t");
			break;
		case TY_UINT64:
			write("uint64_t");
			break;
		case TY_BOOL:
			write("jabool");
			break;
		case TY_STRUCT:
			write("%I", dtype->id);
			break;
		case TY_PTR:
			if(is_dynarray_ptr_type(dtype)) {
				write("jadynarray");
			}
			else {
				gen_type(dtype->subtype);
				if(dtype->subtype->type == TY_ARRAY) write("(");
				write("*");
			}
			break;
		case TY_ARRAY:
			gen_type(dtype->subtype);
			break;
	}
}

static void gen_type_postfix(TypeDesc *dtype)
{
	switch(dtype->type) {
		case TY_PTR:
			if(is_dynarray_ptr_type(dtype)) break;
			if(dtype->subtype->type == TY_ARRAY) write(")");
			gen_type_postfix(dtype->subtype);
			break;
		case TY_ARRAY:
			write("[%u]%z", dtype->length, dtype->subtype);
			break;
	}
}

static void gen_cast(Expr *expr)
{
	TypeDesc *dtype = expr->dtype;
	TypeDesc *subtype = dtype->subtype;
	Expr *srcexpr = expr->subexpr;
	TypeDesc *srctype = srcexpr->dtype;
	
	if(dtype->type == TY_BOOL) {
		write("(%e ? jatrue : jafalse)", srcexpr);
	}
	else if(is_dynarray_ptr_type(dtype)) {
		write("((%Y){.length = ", dtype);
		
		if(srctype->subtype->type == TY_ARRAY) {
			// from static array
			write("%i", srctype->subtype->length);
		}
		else {
			// other
			write("0");
		}
		
		write(", .items = %e})", srcexpr);
	}
	else {
		write("((%Y)%e)", dtype, srcexpr);
	}
}

static void gen_expr(Expr *expr)
{
	switch(expr->type) {
		case EX_INT:
			write("%i", expr->ival);
			break;
		case EX_BOOL:
			write(expr->ival ? "jatrue" : "jafalse");
			break;
		case EX_VAR:
			write("%I", expr->id);
			break;
		case EX_PTR:
			write("(&%e)", expr->subexpr);
			break;
		case EX_DEREF:
			write("(*%e)", expr->subexpr);
			break;
		case EX_CAST:
			gen_cast(expr);
			break;
		case EX_SUBSCRIPT:
			if(
				expr->subexpr->type == EX_DEREF &&
				is_dynarray_ptr_type(expr->subexpr->subexpr->dtype)
			) {
				Expr *dynarray = expr->subexpr->subexpr;
				Expr *index = expr->index;
				TypeDesc *itemtype = expr->dtype;
				
				write(
					"(((%y(*)%z)%e.items)[%e])",
					itemtype, itemtype, dynarray, index
				);
			}
			else {
				write("(%e[%e])", expr->subexpr, expr->index);
			}
			break;
		case EX_BINOP:
			write(
				"(%e %s %e)", expr->left, expr->operator->punct, expr->right
			);
			break;
		case EX_ARRAY:
			write("((%Y){", expr->dtype);
			for(Expr *item = expr->exprs; item; item = item->next) {
				if(item != expr->exprs)
					write(", ");
				gen_expr(item);
			}
			write("})");
			break;
		case EX_CALL:
			write("(%e()", expr->callee);
			if(expr->callee->dtype->returntype->type == TY_ARRAY) {
				write(".a");
			}
			write(")");
			break;
		case EX_MEMBER:
			if(
				expr->subexpr->dtype->type == TY_ARRAY &&
				token_text_equals(expr->member_id, "length")
			) {
				if(
					expr->suebxpr->type == EX_DEREF &&
					expr->subexpr->dtype->length == -1 &&
					token_text_equals(expr->member_id, "length")
				) {
					write("(%e.length)", expr->subexpr->subexpr);
				}
				else {
					write("%i", expr->subexpr->dtype->length);
				}
			}
			else {
				write("(%e.%I)", expr->subexpr, expr->member_id);
			}
			break;
	}
}

static void gen_init_expr(Expr *expr)
{
	if(expr->type == EX_ARRAY) {
		write("{");
		for(Expr *item = expr->exprs; item; item = item->next) {
			if(item != expr->exprs)
				write(", ");
			gen_init_expr(item);
		}
		write("}");
	}
	else {
		gen_expr(expr);
	}
}

static void gen_assign(Expr *target, Expr *expr)
{
	if(target->dtype->type == TY_ARRAY) {
		if(expr->type == EX_ARRAY) {
			Expr *item = expr->exprs;
			for(uint64_t i=0; i < expr->length; i++, item = item->next) {
				gen_assign(
					new_subscript(target, new_int_expr(i, target->start)),
					item
				);
			}
		}
		else {
			write(
				"%>memcpy(%e, %e, sizeof(%Y));\n",
				target, expr, target->dtype
			);
		}
	}
	else {
		write("%>%e = %e;\n", target, expr);
	}
}

static void gen_print(Expr *expr)
{
	write("%>printf(");
	
	switch(expr->dtype->type) {
		case TY_INT64:
			write("\"%%\" PRId64");
			break;
		case TY_UINT8:
			write("\"%%\" PRIu8");
			break;
		case TY_UINT64:
			write("\"%%\" PRIu64");
			break;
		case TY_BOOL:
			write("\"%%s\"");
			break;
		case TY_PTR:
			write("\"%%p\"");
			break;
	}
	
	write(" \"\\n\", ");
	
	if(expr->dtype->type == TY_PTR)
		write("(void*)");
	
	gen_expr(expr);
	
	if(expr->dtype->type == TY_BOOL)
		write(" ? \"true\" : \"false\"");
	
	write(");\n");
}

static void gen_stmt(Stmt *stmt)
{
	switch(stmt->type) {
		case ST_PRINT:
			gen_print(stmt->expr);
			break;
		case ST_VARDECL:
			if(stmt->scope->parent) {
				// local var
				gen_vardecl(stmt);
			}
			else {
				// global var
				if(stmt->expr && !stmt->expr->isconst) {
					// with non-constant initializer
					gen_assign(
						new_var_expr(stmt->id, stmt->dtype, stmt->start),
						stmt->expr
					);
				}
			}
			break;
		case ST_IFSTMT:
			write("%>if(%e) {\n", stmt->expr);
			gen_stmts(stmt->if_body);
			write("%>}\n");
			if(stmt->else_body) {
				write("%>else {\n");
				gen_stmts(stmt->else_body);
				write("%>}\n");
			}
			break;
		case ST_WHILESTMT:
			write("%>while(%e) {\n", stmt->expr);
			gen_stmts(stmt->while_body);
			write("%>}\n");
			break;
		case ST_ASSIGN:
			gen_assign(stmt->target, stmt->expr);
			break;
		case ST_CALL:
			write("%>%e;\n", stmt->call);
			break;
		case ST_RETURN:
			if(stmt->expr) {
				Expr *result = stmt->expr;
				TypeDesc *dtype = result->dtype;
				if(dtype->type == TY_ARRAY) {
					if(result->type == EX_ARRAY) {
						write(
							"%>return (rt%I){%E};\n",
							stmt->scope->func->id,
							result
						);
					}
				}
				else {
					write("%>return %e;\n", stmt->expr);
				}
			}
			else {
				write("%>return;\n");
			}
			break;
	}
}

static void gen_stmts(Stmt *stmts)
{
	if(!stmts) return;
	
	level ++;
	
	for(Stmt *stmt = stmts; stmt; stmt = stmt->next) {
		gen_stmt(stmt);
	}
	
	level --;
}

static void gen_structdecl(Stmt *stmt)
{
	write("%>typedef struct {\n");
	level ++;
	gen_vardecls(stmt->struct_body);
	level --;
	write("%>} %I;\n", stmt->id);
}

static void gen_vardecl(Stmt *stmt)
{
	write("%>");
	if(!stmt->scope->parent && !stmt->scope->struc) write("static ");
	write("%y %I%z", stmt->dtype, stmt->id, stmt->dtype);
	
	if(stmt->scope->struc) {
		write(";\n");
	}
	else if(stmt->expr) {
		// has initializer
		if(
			stmt->expr->isconst ||
			stmt->scope->parent && stmt->expr->type != EX_ARRAY
		) {
			// is constant or for local var (no array literal)
			// => in-place init possible
			write(" = ");
			gen_init_expr(stmt->expr);
			write(";\n");
		}
		else if(stmt->scope->parent && stmt->expr->type == EX_ARRAY) {
			// array literal initializer for local var
			write(";\n");
			gen_assign(
				new_var_expr(stmt->id, stmt->dtype, stmt->start),
				stmt->expr
			);
		}
		else {
			write(";\n");
		}
	}
	else if(
		stmt->dtype->type == TY_ARRAY || stmt->dtype->type == TY_STRUCT ||
		is_dynarray_ptr_type(stmt->dtype)
	) {
		write(" = {0};\n");
	}
	else {
		write(" = 0;\n");
	}
}

static void gen_funcdecl(Stmt *stmt)
{
	TypeDesc *returntype = stmt->dtype;
	
	if(returntype->type == TY_ARRAY) {
		write(
			"%>typedef struct { %y a%z; } rt%I;\n",
			returntype, returntype, stmt->id
		);
		write("%>static rt%I %I() {\n", stmt->id, stmt->id);
	}
	else {
		write("%>static %y %I()%z {\n", returntype, stmt->id, returntype);
	}
	
	gen_stmts(stmt->func_body);
	write("%>}\n");
}

static void gen_structdecls(Stmt *stmts)
{
	for(Stmt *stmt = stmts; stmt; stmt = stmt->next) {
		if(stmt->type == ST_STRUCTDECL) {
			gen_structdecl(stmt);
		}
	}
}

static void gen_vardecls(Stmt *stmts)
{
	for(Stmt *stmt = stmts; stmt; stmt = stmt->next) {
		if(stmt->type == ST_VARDECL) {
			gen_vardecl(stmt);
		}
	}
}

static void gen_funcdecls(Stmt *stmts)
{
	for(Stmt *stmt = stmts; stmt; stmt = stmt->next) {
		if(stmt->type == ST_FUNCDECL) {
			gen_funcdecl(stmt);
		}
	}
}

void gen(Unit *unit)
{
	ofs = fopen(unit->c_filename, "wb");
	level = 0;
	
	write(
		"#include <stdio.h>\n"
		"#include <stdint.h>\n"
		"#include <inttypes.h>\n"
		"#include <string.h>\n"
		"#define jafalse ((jabool)0)\n"
		"#define jatrue ((jabool)1)\n"
		"typedef uint8_t jabool;\n"
		"typedef struct {\n"
		INDENT "int64_t length;\n"
		INDENT "void *items;\n"
		"} jadynarray;\n"
	);
	
	gen_structdecls(unit->stmts);
	gen_vardecls(unit->stmts);
	gen_funcdecls(unit->stmts);
	write("int main(int argc, char **argv) {\n");
	gen_stmts(unit->stmts);
	write("}\n");
	fclose(ofs);
}
