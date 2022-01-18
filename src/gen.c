#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include "gen.h"

static FILE *ofs;
static int64_t level;

static void gen_stmts(Stmt *stmts);
static void gen_vardecls(Stmt *stmts);
static void gen_vardecl(Stmt *stmt);

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

static void gen_ident(Token *id)
{
	write("ja_");
	fwrite(id->start, 1, id->length, ofs);
	
	fwrite(id->start, 1, id->length, stdout);
}

static void gen_type(TypeDesc *dtype)
{
	switch(dtype->type) {
		case TY_INT64:
			write("int64_t");
			break;
		case TY_BOOL:
			write("jabool");
			break;
		case TY_PTR:
			gen_type(dtype->subtype);
			write("*");
			break;
	}
}

static void gen_expr(Expr *expr)
{
	switch(expr->type) {
		case EX_INT:
			write("INT64_C(%" PRId64 ")", expr->ival);
			break;
		case EX_BOOL:
			write(expr->bval ? "jatrue" : "jafalse");
			break;
		case EX_VAR:
			gen_ident(expr->id);
			break;
		case EX_PTR:
			write("&");
			gen_expr(expr->expr);
			break;
	}
}

static void gen_stmt(Stmt *stmt)
{
	switch(stmt->type) {
		case ST_PRINT:
			gen_indent();
			write("printf(");
			
			switch(stmt->expr->dtype->type) {
				case TY_INT64:
					write("\"%\" PRId64");
					break;
				case TY_BOOL:
					write("\"%%s\"");
					break;
			}
			
			write(" \"\\n\", ");
			gen_expr(stmt->expr);
			
			if(stmt->expr->dtype->type == TY_BOOL)
				write(" ? \"true\" : \"false\"");
			
			write(");\n");
			break;
		case ST_VARDECL:
			if(stmt->scope->parent) {
				gen_vardecl(stmt);
			}
			else if(stmt->expr && !stmt->expr->isconst) {
				gen_indent();
				gen_ident(stmt->id);
				write(" = ");
				gen_expr(stmt->expr);
				write(";\n");
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
	gen_type(stmt->dtype);
	write(" ");
	gen_ident(stmt->id);
	
	if(stmt->expr) {
		if(stmt->expr->isconst || stmt->scope->parent) {
			write(" = ");
			gen_expr(stmt->expr);
		}
	}
	else {
		write(" = 0");
	}
	
	write(";\n");
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
	ofs = stdout;
	ofs = fopen("./output.c", "wb");
	level = 0;
	write("#include <stdio.h>\n");
	write("#include <stdint.h>\n");
	write("#include <inttypes.h>\n");
	write("#define jafalse ((jabool)0)\n");
	write("#define jatrue ((jabool)1)\n");
	write("typedef uint8_t jabool;\n");
	gen_vardecls(unit->stmts);
	write("int main(int argc, char **argv) {\n");
	gen_stmts(unit->stmts);
	write("}\n");
}
