#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include "gen.h"

#define COL_YELLOW  "\x1b[38;2;255;255;0m"
#define COL_RESET   "\x1b[0m"

static FILE *ofs;
static int64_t level;

static void gen_stmts(Stmt *stmts);
static void gen_vardecls(Stmt *stmts);
static void gen_vardecl(Stmt *stmt);
static void gen_assign(Expr *target, Expr *expr);

static void write(char *msg, ...)
{
	va_list args;
	va_start(args, msg);
	vfprintf(ofs, msg, args);
	va_end(args);
	
	va_start(args, msg);
	vfprintf(stdout, msg, args);
	va_end(args);
}

static void gen_indent()
{
	for(int64_t i=0; i<level; i++) write("    ");
}

static void gen_int(uint64_t val)
{
	write("%" PRId64, val);
}

static void gen_uint(uint64_t val)
{
	write("%" PRIu64, val);
}

static void gen_ident(Token *id)
{
	write("ja_");
	for(int64_t i=0; i < id->length; i++) write("%c", id->start[i]);
}

static void gen_type(TypeDesc *dtype)
{
	switch(dtype->type) {
		case TY_INT64:
			write("int64_t");
			break;
		case TY_UINT64:
			write("uint64_t");
			break;
		case TY_BOOL:
			write("jabool");
			break;
		case TY_PTR:
			gen_type(dtype->subtype);
			write("(*");
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
			write(")");
			gen_type_postfix(dtype->subtype);
			break;
		case TY_ARRAY:
			write("[");
			gen_uint(dtype->length);
			write("]");
			gen_type_postfix(dtype->subtype);
			break;
	}
}

static void gen_expr(Expr *expr)
{
	switch(expr->type) {
		case EX_INT:
			gen_int(expr->ival);
			break;
		case EX_BOOL:
			write(expr->ival ? "jatrue" : "jafalse");
			break;
		case EX_VAR:
			gen_ident(expr->id);
			break;
		case EX_PTR:
			write("&");
			gen_expr(expr->subexpr);
			break;
		case EX_DEREF:
			write("*");
			gen_expr(expr->subexpr);
			break;
		case EX_CAST:
			if(expr->dtype->type == TY_BOOL) {
				write("(");
				gen_expr(expr->subexpr);
				write(" ? jatrue : jafalse)");
			}
			else {
				write("(");
				gen_type(expr->dtype);
				write(")");
				gen_expr(expr->subexpr);
			}
			break;
		case EX_SUBSCRIPT:
			gen_expr(expr->subexpr);
			write("[");
			gen_expr(expr->index);
			write("]");
			break;
		case EX_BINOP:
			gen_expr(expr->left);
			write(" %s ", expr->operator->punct);
			gen_expr(expr->right);
			break;
		case EX_ARRAY:
			write("(");
			gen_type(expr->dtype);
			gen_type_postfix(expr->dtype);
			write(")");
			write("{");
			for(Expr *item = expr->exprs; item; item = item->next) {
				if(item != expr->exprs)
					write(", ");
				gen_expr(item);
			}
			write("}");
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
			gen_indent();
			write("memcpy(");
			gen_expr(target);
			write(", ");
			gen_expr(expr);
			write(", sizeof(");
			gen_type(target->dtype);
			gen_type_postfix(target->dtype);
			write("));\n");
		}
	}
	else {
		gen_indent();
		gen_expr(target);
		write(" = ");
		gen_expr(expr);
		write(";\n");
	}
}

static void gen_print(Expr *expr)
{
	gen_indent();
	write("printf(");
	
	switch(expr->dtype->type) {
		case TY_INT64:
			write("\"%\" PRId64");
			break;
		case TY_UINT64:
			write("\"%\" PRIu64");
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
			gen_indent();
			write("if(");
			gen_expr(stmt->expr);
			write(") {\n");
			gen_stmts(stmt->body);
			gen_indent();
			write("}\n");
			if(stmt->else_body) {
				gen_indent();
				write("else {\n");
				gen_stmts(stmt->else_body);
				gen_indent();
				write("}\n");
			}
			break;
		case ST_WHILESTMT:
			gen_indent();
			write("while(");
			gen_expr(stmt->expr);
			write(") {\n");
			gen_stmts(stmt->body);
			gen_indent();
			write("}\n");
			break;
		case ST_ASSIGN:
			gen_assign(stmt->target, stmt->expr);
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

static void gen_vardecl(Stmt *stmt)
{
	gen_indent();
	
	if(!stmt->scope->parent)
		write("static ");
	
	gen_type(stmt->dtype);
	write(" ");
	gen_ident(stmt->id);
	gen_type_postfix(stmt->dtype);
	
	if(stmt->expr) {
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
	else if(stmt->dtype->type == TY_ARRAY) {
		write(" = {0};\n");
	}
	else {
		write(" = 0;\n");
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

void gen(Unit *unit)
{
	printf(COL_YELLOW "=== code ===" COL_RESET "\n");
	ofs = stdout;
	ofs = fopen("./output.c", "wb");
	level = 0;
	write("#include <stdio.h>\n");
	write("#include <stdint.h>\n");
	write("#include <inttypes.h>\n");
	write("#include <string.h>\n");
	write("#define jafalse ((jabool)0)\n");
	write("#define jatrue ((jabool)1)\n");
	write("typedef uint8_t jabool;\n");
	gen_vardecls(unit->stmts);
	write("int main(int argc, char **argv) {\n");
	gen_stmts(unit->stmts);
	write("}\n");
}
