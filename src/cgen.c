#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include "ast.h"
#include "cgen_internal.h"
#include "array.h"
#include "string.h"

static Unit *cur_unit;
static FILE *ofs;
static int64_t level;
static int in_header;

static void gen_vardecls(Decl **decls);
static void gen_vardecl_stmt(Decl *decl);

int is_in_header()
{
	return in_header;
}

Unit *get_cur_unit()
{
	return cur_unit;
}

void gen_mainfuncname(Unit *unit)
{
	write("_%s_main", unit->unit_id);
}

void inc_level()
{
	level++;
}

void dec_level()
{
	level--;
}

static void gen_mainfunchead(Unit *unit)
{
	write("int ");
	gen_mainfuncname(unit);
	write("(int argc, char **argv)");
}

/*
	format specifiers:
		%% - literal %
		%> - indentation of the current level
		%c - byte character
		%s - null terminated char*
		%t - Token*
		%y - prefix of a Type*
		%z - postfix of a Type*
		%Y - complete Type*
		%e - Expr*
		%E - init Expr*
		%S - a char* string followed by a int64_t length
		%I - ja_ identifier Token*
		%X - _<unit-id>_ prefixed Token*
		%i - signed 64 bit integer
		%u - unsigned 64 bit integer
*/
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

void gen_stmts(Stmt **stmts)
{
	if(!stmts) return;
	
	level ++;
	
	array_for(stmts, i) {
		gen_stmt(stmts[i], 0);
	}
	
	level --;
}

void gen_block(Block *block)
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
		Type *type = param->type;
		if(i > 0) write(", ");
		
		if(type->kind == ARRAY) {
			write("%y (*ap_%t)%z", type, param->id, type);
		}
		else {
			write("%y %s%z", type, param->private_id, type);
		}
	}
}

static void gen_array_param_decls(Decl **params)
{
	level ++;
	
	array_for(params, i) {
		Decl *param = params[i];
		Type *type = param->type;
		
		if(type->kind == ARRAY) {
			write("%>%y %s%z;\n", type, param->private_id, type);
			
			write(
				"%>memcpy(%s, ap_%t, sizeof(%Y) * %i);\n",
				param->private_id, param->id, type->itemtype, type->length
			);
		}
	}
	
	level --;
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
	
void gen_vardecl_init(Decl *decl, int struct_inst_member)
{
	if(in_header || decl->scope->structhost && !struct_inst_member)
		return;
	
	if(decl->init) {
		if(
			decl->init->isconst ||
			(decl->scope->parent && decl->init->type->kind != ARRAY)
		) {
			write(" = ");
			gen_init_expr(decl->init);
		}
	}
	else if(decl->type->kind == STRUCT) {
		Decl *structdecl = decl->type->decl;
		write(" = {");
		
		array_for(structdecl->members, i) {
			if(i > 0) write(", ");
			Decl *member = structdecl->members[i];
			
			write(".%s", member->private_id);
			gen_vardecl_init(member, 1);
		}
		
		write("}");
	}
	else if(
		decl->type->kind == ARRAY || decl->type->kind == UNION ||
		decl->type->kind == STRING || decl->type->kind == SLICE
	) {
		write(" = {0}");
	}
	else {
		write(" = 0");
	}
}

// --- //

static void gen_enumdecl(Decl *decl)
{
	if(decl->imported)
		return;
	
	if(!in_header && decl->exported) {
		gen_export_alias(decl);
		return;
	}
	
	if(in_header && !decl->exported)
		return;
	
	write("%>typedef enum {\n");
	level ++;
	EnumItem **items = decl->items;
	
	array_for(items, i) {
		if(in_header)
			write(
				"%>_%s_ja_%t = %e,\n",
				decl->public_id, items[i]->id, items[i]->val
			);
		else
			write(
				"%>_%s_ja_%t = %e,\n",
				decl->private_id, items[i]->id, items[i]->val
			);
	}
	
	level --;
	
	if(in_header)
		write("%>} %s;\n", decl->public_id);
	else
		write("%>} %s;\n", decl->private_id);
}

static void gen_structdecl_typedef(Decl *decl)
{
	if(decl->imported)
		return;
	
	if(!in_header && decl->exported) {
		gen_export_alias(decl);
		return;
	}
	
	if(in_header && !decl->exported)
		return;
	
	if(decl->kind == STRUCT)
		write("%>typedef struct ");
	else if(decl->kind == UNION)
		write("%>typedef union ");
	
	if(in_header)
		write("%s %s;\n", decl->public_id, decl->public_id);
	else
		write("%s %s;\n", decl->private_id, decl->private_id);
}

static void gen_structdecl(Decl *decl)
{
	if(decl->imported)
		return;
	
	if(!in_header && decl->exported) {
		return;
	}
	
	if(in_header && !decl->exported)
		return;
	
	if(decl->kind == STRUCT)
		write("%>struct ");
	else if(decl->kind == UNION)
		write("%>union ");
	
	if(in_header)
		write("%s {\n", decl->public_id);
	else
		write("%s {\n", decl->private_id);
	
	level ++;
	gen_vardecls(decl->members);
	level --;
	
	write("%>};\n");
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
	gen_array_param_decls(decl->params);
	gen_block(decl->body);
	write("%>}\n");
}

void gen_vardecl(Decl *decl)
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
	
	gen_vardecl_init(decl, 0);
	
	write(";\n");
}

// --- //

static void gen_enumdecls(Decl **decls)
{
	array_for(decls, i) {
		if(decls[i]->kind == ENUM) {
			gen_enumdecl(decls[i]);
		}
	}
}

static void gen_structdecls(Decl **decls)
{
	array_for(decls, i) {
		if(decls[i]->kind == STRUCT || decls[i]->kind == UNION) {
			gen_structdecl_typedef(decls[i]);
		}
	}

	array_for(decls, i) {
		if(decls[i]->kind == STRUCT || decls[i]->kind == UNION) {
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
		
		array_for(decls, j) {
			gen_export_alias(decls[j]);
		}
	}
}

static void gen_foreign_decls(Foreign **imports)
{
	array_for(imports, i) {
		Foreign *import = imports[i];
		Decl **decls = import->decls;
		
		array_for(decls, j) {
			Decl *decl = decls[j];
			
			if(decl->kind == FUNC) {
				Type *returntype = decl->type->returntype;
				write("static %y (*%s)(", returntype, decl->private_id);
				gen_params(decl->params);
				write(")%z;\n", returntype);
			}
		}
	}
}

static void gen_foreign_imports(Foreign **imports)
{
	write(INDENT "void *lib = 0;\n");
	
	array_for(imports, i) {
		Foreign *import = imports[i];
		Decl **decls = import->decls;
		
		write(
			INDENT "lib = dlopen(\"%s\", RTLD_LAZY);\n"
			INDENT "if(lib == 0) {\n"
			INDENT INDENT "fprintf(stderr, \"error: could not load library "
				"%s\\n\");\n"
			INDENT "}\n"
			, import->filename
			, import->filename
		);
		
		array_for(decls, j) {
			Decl *decl = decls[j];
			
			if(decl->kind == FUNC) {
				write(
					INDENT "*(void**)&%s = dlsym(lib, \"%t\");\n"
					,decl->private_id, decl->id
				);
			}
			else if(decl->kind == VAR) {
				write(
					INDENT "%s = dlsym(lib, \"%t\");\n",
					decl->private_id, decl->id
				);
			}
			
			write(
				INDENT "if(%s == 0) {\n"
				INDENT INDENT "fprintf(stderr, \"error: could not load symbol "
					"%t\\n\");\n"
				INDENT "}\n"
				, decl->private_id
				, decl->id
			);
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
	
	write("\n// exported enums\n");
	gen_enumdecls(decls);
	
	write("\n// exported structures & unions\n");
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
	
	write("\n// imports\n");
	gen_imports(unit_scope->imports);
	
	write("\n// enums\n");
	gen_enumdecls(decls);
	
	write("\n// structures & unions\n");
	gen_structdecls(decls);
	
	write("\n// foreign imports\n");
	gen_foreign_decls(unit_scope->foreigns);
	
	write("\n// variables\n");
	gen_vardecls(decls);
	
	write("\n// function prototypes\n");
	gen_funcprotos(decls);
	
	write("\n// function implementations\n");
	gen_funcdecls(decls);
	
	write(
		"\n// control variables\n"
		"static int main_was_called;\n\n"
		"// main function\n"
	);
	
	gen_mainfunchead(cur_unit);
	level ++;
	
	write(
		" {\n"
		"%>if(main_was_called) return 0;\n"
		"%>else main_was_called = 1;\n"
		"%>jastring argv_buf[argc];\n"
		"%>ja_argv = (jaslice){.length = argc, .items = argv_buf};\n"
		"%>for(int64_t i=0; i < argc; i++) "
		"((jastring*)ja_argv.items)[i] = "
			"(jastring){strlen(argv[i]), argv[i]};\n"
	);
	
	write("%>// foreign imports\n");
	gen_foreign_imports(unit_scope->foreigns);
	level --;
	gen_block(cur_unit->block);
	
	write(
		INDENT "return 0;\n"
		"}\n"
	);
	
	if(cur_unit->ismain) {
		write(
			"\n#ifdef JA_ISMAIN\n"
			"int main(int argc, char **argv) {\n"
			INDENT "return ",
			cur_unit->h_filename
		);
		
		gen_mainfuncname(cur_unit);
		
		write(
			"(argc, argv);\n"
			"}\n"
			"#endif\n"
		);
	}
	
	fclose(ofs);
}

// --- //

void gen(Unit *unit)
{
	cur_unit = unit;
	gen_h();
	gen_c();
}
