#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include "cgen.h"
#include "cgen_utils.h"
#include "utils.h"

#define INDENT      "    "
#define COL_YELLOW  "\x1b[38;2;255;255;0m"
#define COL_RESET   "\x1b[0m"

static Unit *cur_unit;
static FILE *ofs;
static int64_t level;
static int in_header;

static void gen_block(Block *block);
static void gen_stmts(Stmt **stmts);
static void gen_vardecls(Decl **decls);
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

static void gen_mainfuncname(Unit *unit)
{
	write("_%s_main", unit->unit_id);
}

static void gen_mainfunchead(Unit *unit)
{
	write("int ");
	gen_mainfuncname(unit);
	write("(int argc, char **argv)");
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
				Type *type = va_arg(args, Type*);
				gen_type(type);
			}
			else if(*msg == 'z') {
				msg++;
				Type *type = va_arg(args, Type*);
				gen_type_postfix(type);
			}
			else if(*msg == 'Y') {
				msg++;
				Type *type = va_arg(args, Type*);
				gen_type(type);
				gen_type_postfix(type);
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

static void gen_print(Expr *expr)
{
	if(expr->type->kind == STRING) {
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
	
	write(" \"\\n\", ");
	
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
			if(result->kind == ARRAY) {
				write(
					"%>return (rt%I){%E};\n",
					returnstmt->scope->funchost->id,
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
				new_var_expr(decl->start, decl),
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

static void gen_block(Block *block)
{
	if(block) gen_stmts(block->stmts);
}

static void gen_export_alias(Decl *decl)
{
	write("#define %s %s\n", decl->private_id, decl->public_id);
}

static void gen_params(Decl **params)
{
	array_for(params, i) {
		Decl *param = params[i];
		if(i > 0) write(", ");
		write("%y %s%z", param->type, param->private_id, param->type);
	}
}

// --- //

static void gen_returntypedecl(Decl *decl)
{
	Type *returntype = decl->type->returntype;
	
	if(returntype->kind == ARRAY) {
		write("%>typedef struct { %y a%z; } ", returntype, returntype);
		
		if(in_header)
			write("rt_%s;\n", decl->public_id);
		else
			write("rt_%s;\n", decl->private_id);
	}
}

static void gen_funchead(Decl *decl)
{
	Type *returntype = decl->type->returntype;
	
	if(!decl->exported)
		write("%>static ");
	else
		write("%>");
	
	if(returntype->kind == ARRAY) {
		if(in_header)
			write("rt_%s %s(", decl->public_id, decl->public_id);
		else
			write("rt_%s %s(", decl->private_id, decl->private_id);
		
		gen_params(decl->params);
		write(")");
	}
	else {
		if(in_header)
			write("%y %s(", returntype, decl->public_id);
		else
			write("%y %s(", returntype, decl->private_id);
		
		gen_params(decl->params);
		write(")%z", returntype);
	}
}
	
void gen_vardecl_init(Decl *decl)
{
	if(in_header || decl->scope->structhost)
		return;
	
	if(decl->init) {
		if(decl->init->isconst || decl->scope->parent) {
			write(" = ");
			gen_init_expr(decl->init);
		}
	}
	else if(
		decl->type->kind == ARRAY || decl->type->kind == STRUCT ||
		decl->type->kind == STRING || is_dynarray_ptr_type(decl->type)
	) {
		write(" = {0}");
	}
	else {
		write(" = 0");
	}
}

// --- //

static void gen_structdecl(Decl *decl)
{
	if(decl->imported)
		return;
	
	if(!in_header && decl->exported) {
		gen_export_alias(decl);
		return;
	}
	
	if(in_header && !decl->exported)
		return;
	
	write("%>typedef struct {\n");
	level ++;
	gen_vardecls(decl->members);
	level --;
	
	if(in_header)
		write("%>} %s;\n", decl->public_id);
	else
		write("%>} %s;\n", decl->private_id);
}

static void gen_funcproto(Decl *decl)
{
	if(decl->imported)
		return;
	
	if(!in_header && decl->exported) {
		gen_export_alias(decl);
		Type *returntype = decl->type->returntype;
		
		if(returntype->kind == ARRAY)
			write("#define rt_%s rt_%s\n", decl->private_id, decl->public_id);
		
		return;
	}
	
	if(in_header && !decl->exported)
		return;
	
	gen_returntypedecl(decl);
	gen_funchead(decl);
	write(";\n");
}

static void gen_funcdecl(Decl *decl)
{
	if(decl->imported)
		return;
	
	gen_funchead(decl);
	write(" {\n");
	gen_block(decl->body);
	write("%>}\n");
}

static void gen_vardecl(Decl *decl)
{
	if(decl->imported)
		return;
	
	if(in_header && !decl->exported && !decl->scope->structhost)
		return;
	
	if(!in_header && decl->exported)
		gen_export_alias(decl);
	
	write("%>");
	
	if(in_header && decl->exported)
		write("extern ");
	
	if(!decl->exported && !decl->scope->parent)
		write("static ");
	
	if(decl->exported && in_header)
		write("%y %s%z", decl->type, decl->public_id, decl->type);
	else
		write("%y %s%z", decl->type, decl->private_id, decl->type);
	
	gen_vardecl_init(decl);
	
	write(";\n");
}

// --- //

static void gen_structdecls(Decl **decls)
{
	array_for(decls, i) {
		if(decls[i]->kind == STRUCT) {
			gen_structdecl(decls[i]);
		}
	}
}

static void gen_funcprotos(Decl **decls)
{
	array_for(decls, i) {
		if(decls[i]->kind == FUNC) {
			gen_funcproto(decls[i]);
		}
	}
}

static void gen_funcdecls(Decl **decls)
{
	array_for(decls, i) {
		if(decls[i]->kind == FUNC) {
			gen_funcdecl(decls[i]);
		}
	}
}

static void gen_vardecls(Decl **decls)
{
	array_for(decls, i) {
		if(decls[i]->kind == VAR) {
			gen_vardecl(decls[i]);
		}
	}
}

static void gen_imports(Import **imports)
{
	array_for(imports, i) {
		Import *import = imports[i];
		write("#include \"%s\"\n", import->unit->h_filename);
		Decl **decls = import->decls;
		
		array_for(decls, i) {
			gen_export_alias(decls[i]);
		}
	}
}

static void gen_dll_import_decls(DllImport **imports)
{
	array_for(imports, i) {
		DllImport *import = imports[i];
		Decl **decls = import->decls;
		
		array_for(decls, i) {
			Decl *decl = decls[i];
			
			if(decl->kind == FUNC) {
				Type *returntype = decl->type->returntype;
				write("static %y (*%s)(", returntype, decl->private_id);
				gen_params(decl->params);
				write(")%z;\n", returntype);
			}
		}
	}
}

static void gen_dll_imports(DllImport **imports)
{
	write(INDENT "void *dll = 0;\n");
	
	array_for(imports, i) {
		DllImport *import = imports[i];
		Decl **decls = import->decls;
		write(INDENT "dll = dlopen(\"%s\", RTLD_LAZY);\n", import->dll_name);
		
		array_for(decls, i) {
			Decl *decl = decls[i];
			
			if(decl->kind == FUNC) {
				write(
					INDENT "*(void**)&(%s) = dlsym(dll, \"%t\");\n",
					decl->private_id, decl->id
				);
			}
			else if(decl->kind == VAR) {
				write(
					INDENT "(%s) = dlsym(dll, \"%t\");\n",
					decl->private_id, decl->id
				);
			}
		}
	}
}

// --- //

static void gen_h()
{
	ofs = fopen(cur_unit->h_filename, "wb");
	level = 0;
	in_header = 1;
	
	write(
		"#ifndef _%s_H\n"
		"#define _%s_H\n\n"
		"// runtime\n"
		"#include \"runtime.h\"\n",
		cur_unit->unit_id, cur_unit->unit_id
	);
	
	write("\n// main function\n");
	gen_mainfunchead(cur_unit);
	write(";\n");
	
	Decl **decls = cur_unit->block->scope->decls;
	
	write("\n// exported structures\n");
	gen_structdecls(decls);
	
	write("\n// exported functions\n");
	gen_funcprotos(decls);
	
	write("\n// exported variables\n");
	gen_vardecls(decls);
	
	write("\n#endif\n");
	
	fclose(ofs);
}

static void gen_c()
{
	ofs = fopen(cur_unit->c_filename, "wb");
	level = 0;
	in_header = 0;
	
	write("#include \"%s\"\n", cur_unit->h_filename);
	
	Scope *unit_scope = cur_unit->block->scope;
	Decl **decls = unit_scope->decls;
	
	write("\n// dll imports\n");
	gen_dll_import_decls(unit_scope->dll_imports);
	
	write("\n// imports\n");
	gen_imports(unit_scope->imports);
	
	write("\n// structures\n");
	gen_structdecls(decls);
	
	write("\n// variables\n");
	gen_vardecls(decls);
	
	write("\n// function prototypes\n");
	gen_funcprotos(decls);
	
	write("\n// function implementations\n");
	gen_funcdecls(decls);
	
	write(
		"\n// control variables\n"
		"static jadynarray ja_argv;\n"
		"static int main_was_called;\n\n"
		"// main function\n"
	);
	
	gen_mainfunchead(cur_unit);
	
	write(
		" {\n"
		INDENT "if(main_was_called) {\n"
		INDENT INDENT "return 0;\n"
		INDENT "}\n"
		INDENT "main_was_called = 1;\n"
		INDENT "ja_argv.length = argc;\n"
		INDENT "ja_argv.items = malloc(sizeof(jastring) * argc);\n"
		INDENT "for(int64_t i=0; i < argc; i++) {\n"
		INDENT INDENT "((jastring*)ja_argv.items)[i] = "
			"(jastring){strlen(argv[i]), argv[i]};\n"
		INDENT "}\n"
	);
	
	write(INDENT "// dll imports\n");
	gen_dll_imports(unit_scope->dll_imports);
	
	gen_block(cur_unit->block);
	
	write("}\n");
	
	fclose(ofs);
}

static void gen_c_main()
{
	ofs = fopen(cur_unit->c_main_filename, "wb");
	level = 0;
	in_header = 0;
	
	write(
		"#include \"%s\"\n"
		"int main(int argc, char **argv) {\n"
		INDENT "return ",
		cur_unit->h_filename
	);
	
	gen_mainfuncname(cur_unit);
	
	write(
		"(argc, argv);\n"
		"}\n"
	);
	
	fclose(ofs);
}

// --- //

void gen(Unit *unit)
{
	cur_unit = unit;
	gen_h();
	gen_c();
	if(cur_unit->ismain) gen_c_main();
}
