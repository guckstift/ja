#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include "cgen.h"
#include "cgen_utils.h"
#include "utils.h"

#define INDENT      "    "
#define COL_YELLOW  "\x1b[38;2;255;255;0m"
#define COL_RESET   "\x1b[0m"

static FILE *ofs;
static int64_t level;
static Unit *cur_unit;
static int in_header;

static void gen_stmts(Stmt **stmts);
static void gen_vardecls(Stmt **stmts);
static void gen_vardecl(Decl *decl);
static void gen_stmt(Stmt *stmt, int noindent);

int is_in_header()
{
	return in_header;
}

Unit *get_cur_unit()
{
	return cur_unit;
}

void write(char *msg, ...)
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
				Type *dtype = va_arg(args, Type*);
				gen_type(dtype);
			}
			else if(*msg == 'z') {
				msg++;
				Type *dtype = va_arg(args, Type*);
				gen_type_postfix(dtype);
			}
			else if(*msg == 'Y') {
				msg++;
				Type *dtype = va_arg(args, Type*);
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
			else if(*msg == 'S') {
				msg++;
				char *string = va_arg(args, char*);
				int64_t length = va_arg(args, int64_t);
				fwrite(string, 1, length, ofs);
			}
			else if(*msg == 'I') {
				msg++;
				write("ja_%t", va_arg(args, Token*));
			}
			else if(*msg == 'X') {
				msg++;
				write("_%s_%t", cur_unit->unit_id, va_arg(args, Token*));
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

static void gen_assign(Expr *target, Expr *expr)
{
	if(target->dtype->kind == ARRAY) {
		if(expr->kind == ARRAY) {
			array_for(expr->exprs, i) {
				Expr *item = expr->exprs[i];
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
	if(expr->dtype->kind == STRING) {
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
		write("%>printf(\"\\n\");\n");
		return;
	}
	
	write("%>printf(");
	
	switch(expr->dtype->kind) {
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
	
	write(" \"\\n\", ");
	
	if(expr->dtype->kind == PTR)
		write("(void*)");
	
	gen_expr(expr);
	
	if(expr->dtype->kind == BOOL)
		write(" ? \"true\" : \"false\"");
	
	write(");\n");
}

static void gen_if(If *ifstmt)
{
	write("if(%e) {\n", ifstmt->expr);
	gen_stmts(ifstmt->if_body);
	write("%>}\n");
	
	if(ifstmt->else_body) {
		Stmt **else_body = ifstmt->else_body;
		
		if(array_length(else_body) == 1 && else_body[0]->kind == IF) {
			write("%>else ");
			gen_stmt(ifstmt->else_body[0], 1);
		}
		else {
			write("%>else {\n");
			gen_stmts(ifstmt->else_body);
			write("%>}\n");
		}
	}
}

static void gen_return(Return *returnstmt)
{
	if(returnstmt->expr) {
		Expr *result = returnstmt->expr;
		Type *dtype = result->dtype;
		if(dtype->kind == ARRAY) {
			if(result->kind == ARRAY) {
				write(
					"%>return (rt%I){%E};\n",
					returnstmt->scope->func->id,
					result
				);
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
	}
	else {
		// global var
		if(decl->init && !decl->init->isconst) {
			// with non-constant initializer
			gen_assign(
				new_var_expr(decl->id, decl->dtype, decl, decl->start),
				decl->init
			);
		}
	}
}

static void gen_stmt(Stmt *stmt, int noindent)
{
	switch(stmt->kind) {
		case PRINT:
			gen_print(stmt->as_print.expr);
			break;
		case VAR:
			gen_vardecl_stmt(&stmt->as_decl);
			break;
		case IF:
			if(noindent == 0) write("%>");
			gen_if(&stmt->as_if);
			break;
		case WHILE:
			write("%>while(%e) {\n", stmt->as_while.expr);
			gen_stmts(stmt->as_while.while_body);
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
			write("%>_%s_main(argc, argv);\n", stmt->as_import.unit->unit_id);
			break;
	}
}

static void gen_stmts(Stmt **stmts)
{
	if(!stmts) return;
	
	level ++;
	
	array_for(stmts, i) {
		gen_stmt(stmts[i], 0);
	}
	
	level --;
}

static void gen_export_alias(Token *ident, Unit *unit)
{
	write("#define %I _%s_%t\n", ident, unit->unit_id, ident);
}

static void gen_structdecl(Decl *decl)
{
	if(decl->exported && !in_header) {
		gen_export_alias(decl->id, cur_unit);
	}
	
	write("%>typedef struct {\n");
	level ++;
	gen_vardecls(decl->body);
	level --;
	
	if(decl->exported && in_header) {
		write("%>} %X;\n", decl->id);
	}
	else {
		write("%>} %I;\n", decl->id);
	}
}

static void gen_vardecl(Decl *decl)
{
	if(decl->exported) {
		gen_export_alias(decl->id, cur_unit);
	}
	
	write("%>");
	
	if(!decl->scope->parent && !decl->scope->struc && !decl->exported)
		write("static ");
	
	write("%y %I%z", decl->dtype, decl->id, decl->dtype);
	
	if(decl->scope->struc) {
		write(";\n");
	}
	else if(decl->init) {
		// has initializer
		if(
			decl->init->isconst ||
			decl->scope->parent && decl->init->kind != ARRAY
		) {
			// is constant or for local var (no array literal)
			// => in-place init possible
			write(" = ");
			gen_init_expr(decl->init);
			write(";\n");
		}
		else if(decl->scope->parent && decl->init->kind == ARRAY) {
			// array literal initializer for local var
			write(";\n");
			gen_assign(
				new_var_expr(decl->id, decl->dtype, decl, decl->start),
				decl->init
			);
		}
		else {
			write(";\n");
		}
	}
	else if(
		decl->dtype->kind == ARRAY || decl->dtype->kind == STRUCT ||
		decl->dtype->kind == STRING || is_dynarray_ptr_type(decl->dtype)
	) {
		write(" = {0};\n");
	}
	else {
		write(" = 0;\n");
	}
}

static void gen_func_returntype_decl(Decl *decl)
{
	Type *returntype = decl->dtype;
	if(returntype->kind == ARRAY) {
		if(decl->exported) {
			if(in_header) {
				write(
					"%>typedef struct { %y a%z; } rt%X;\n",
					returntype, returntype, decl->id
				);
			}
			else {
				write("#define rt%I rt%X\n", decl->id, decl->id);
				write(
					"%>typedef struct { %y a%z; } rt%I;\n",
					returntype, returntype, decl->id
				);
			}
		}
		else {
			write(
				"%>typedef struct { %y a%z; } rt%I;\n",
				returntype, returntype, decl->id
			);
		}
	}
}

static void gen_params(Decl *func)
{
	array_for(func->params, i) {
		Decl *param = func->params[i];
		if(i > 0) write(", ");
		write("%y %I%z", param->dtype, param->id, param->dtype);
	}
}

static void gen_func_head(Decl *decl)
{
	Type *returntype = decl->dtype;
	
	if(returntype->kind == ARRAY) {
		if(decl->exported) {
			if(in_header) {
				write("%>rt%X %X(", decl->id, decl->id);
			}
			else {
				write("%>rt%I %I(", decl->id, decl->id);
			}
		}
		else {
			write("%>static rt%I %I(", decl->id, decl->id);
		}
		
		gen_params(decl);
		write(")");
	}
	else {
		if(decl->exported) {
			if(in_header) {
				write("%>%y %X(", returntype, decl->id);
				gen_params(decl);
				write(")%z", returntype);
			}
			else {
				write("%>%y %I(", returntype, decl->id);
				gen_params(decl);
				write(")%z", returntype);
			}
		}
		else {
			write("%>static %y %I(", returntype, decl->id);
			gen_params(decl);
			write(")%z", returntype);
		}
	}
}

static void gen_funcdecl(Decl *decl)
{
	gen_func_head(decl);
	write(" {\n");
	gen_stmts(decl->body);
	write("%>}\n");
}

static void gen_structdecls(Stmt **stmts)
{
	write("// structures\n");
	
	array_for(cur_unit->stmts, i) {
		Stmt *stmt = cur_unit->stmts[i];
		if(stmt->kind == STRUCT) {
			gen_structdecl((Decl*)stmt);
		}
	}
}

static void gen_vardecls(Stmt **stmts)
{
	if(stmts && !stmts[0]->scope->parent) write("// variables\n");
	
	array_for(cur_unit->stmts, i) {
		Stmt *stmt = cur_unit->stmts[i];
		if(stmt->kind == VAR) {
			gen_vardecl((Decl*)stmt);
		}
	}
}

static void gen_funcprotos(Stmt **stmts)
{
	write("// function prototypes\n");
	
	array_for(cur_unit->stmts, i) {
		Stmt *stmt = cur_unit->stmts[i];
		if(stmt->kind == FUNC) {
			Decl *decl = (Decl*)stmt;
			
			if(decl->exported) {
				gen_export_alias(decl->id, cur_unit);
			}
			
			gen_func_returntype_decl(decl);
			gen_func_head(decl);
			write(";\n");
		}
	}
}

static void gen_funcdecls(Stmt **stmts)
{
	write("// functions\n");
	
	array_for(cur_unit->stmts, i) {
		Stmt *stmt = cur_unit->stmts[i];
		if(stmt->kind == FUNC) {
			gen_funcdecl((Decl*)stmt);
		}
	}
}

static void gen_imports(Stmt **stmts)
{
	write("// imports\n");
	
	array_for(cur_unit->stmts, i) {
		Stmt *stmt = cur_unit->stmts[i];
		if(stmt->kind == IMPORT) {
			write("#include \"%s\"\n", stmt->as_import.unit->h_filename);
			
			for(int64_t i=0; i < stmt->as_import.imported_ident_count; i++) {
				Token *ident = stmt->as_import.imported_idents + i * 2;
				gen_export_alias(ident, stmt->as_import.unit);
			}
		}
	}
}

static void gen_h()
{
	in_header = 1;
	ofs = fopen(cur_unit->h_filename, "wb");

	write("// runtime\n");
	write("#include \"runtime.h\"\n");
	
	write("// main function\n");
	write("int _%s_main(int argc, char **argv);\n", cur_unit->unit_id);
	
	write("// exported structures\n");
	array_for(cur_unit->stmts, i) {
		Stmt *stmt = cur_unit->stmts[i];
		if(stmt->kind == STRUCT && stmt->as_decl.exported) {
			gen_structdecl((Decl*)stmt);
		}
	}
	
	write("// exported functions\n");
	array_for(cur_unit->stmts, i) {
		Stmt *stmt = cur_unit->stmts[i];
		if(stmt->kind == FUNC && stmt->as_decl.exported) {
			gen_func_returntype_decl((Decl*)stmt);
			gen_func_head((Decl*)stmt);
			write(";\n");
		}
	}
	
	write("// exported variables\n");
	array_for(cur_unit->stmts, i) {
		Stmt *stmt = cur_unit->stmts[i];
		if(stmt->kind == VAR && stmt->as_decl.exported) {
			write(
				"extern %y %X%z;\n",
				stmt->as_decl.dtype, stmt->as_decl.id, stmt->as_decl.dtype
			);
		}
	}
	
	fclose(ofs);
	in_header = 0;
}

static void gen_c()
{
	ofs = fopen(cur_unit->c_filename, "wb");
	level = 0;
	
	write("// runtime\n");
	write("#include \"runtime.h\"\n");
	
	gen_imports(cur_unit->stmts);
	gen_structdecls(cur_unit->stmts);
	gen_vardecls(cur_unit->stmts);
	gen_funcprotos(cur_unit->stmts);
	gen_funcdecls(cur_unit->stmts);
	
	write("// control variables\n");
	write("static jadynarray ja_argv;\n");
	write("static int main_was_called;\n");
	write("// main function\n");
	write("int _%s_main(int argc, char **argv) {\n", cur_unit->unit_id);
	
	write(
		INDENT "if(main_was_called) return 0;\n"
		INDENT "main_was_called = 1;\n"
		INDENT "ja_argv.length = argc;\n"
		INDENT "ja_argv.items = malloc(sizeof(jastring) * argc);\n"
		INDENT "for(int64_t i=0; i < argc; i++) "
			"((jastring*)ja_argv.items)[i] = "
			"(jastring){strlen(argv[i]), argv[i]};\n"
	);
	
	gen_stmts(cur_unit->stmts);
	write("}\n");
	
	fclose(ofs);
}

static void gen_main_c()
{
	if(!cur_unit->ismain) return;
	
	ofs = fopen(cur_unit->c_main_filename, "wb");
	
	write(
		"#include \"%s\"\n"
		"int main(int argc, char **argv) {\n"
		INDENT "_%s_main(argc, argv);\n"
		"}\n",
		cur_unit->h_filename,
		cur_unit->unit_id
	);
	
	fclose(ofs);
}

void gen(Unit *unit)
{
	cur_unit = unit;
	gen_h();
	gen_c();
	gen_main_c();
}
