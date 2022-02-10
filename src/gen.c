#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include "gen.h"
#include "runtime.inc.h"

#define INDENT      "    "
#define COL_YELLOW  "\x1b[38;2;255;255;0m"
#define COL_RESET   "\x1b[0m"

static FILE *ofs;
static int64_t level;
static Unit *cur_unit;

static void gen_stmts(Stmt *stmts);
static void gen_vardecls(Stmt *stmts);
static void gen_vardecl(Stmt *stmt);
static void gen_assign(Expr *target, Expr *expr);
static void gen_type(Type *dtype);
static void gen_type_postfix(Type *dtype);
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

static void gen_type(Type *dtype)
{
	switch(dtype->kind) {
		case NONE:
			write("void");
			break;
		case INT8:
			write("int8_t");
			break;
		case INT16:
			write("int16_t");
			break;
		case INT32:
			write("int32_t");
			break;
		case INT64:
			write("int64_t");
			break;
		case UINT8:
			write("uint8_t");
			break;
		case UINT16:
			write("uint16_t");
			break;
		case UINT32:
			write("uint32_t");
			break;
		case UINT64:
			write("uint64_t");
			break;
		case BOOL:
			write("jabool");
			break;
		case STRING:
			write("jastring");
			break;
		case STRUCT:
			write("%I", dtype->id);
			break;
		case PTR:
			if(is_dynarray_ptr_type(dtype)) {
				write("jadynarray");
			}
			else {
				gen_type(dtype->subtype);
				if(dtype->subtype->kind == ARRAY) write("(");
				write("*");
			}
			break;
		case ARRAY:
			gen_type(dtype->itemtype);
			break;
	}
}

static void gen_type_postfix(Type *dtype)
{
	switch(dtype->kind) {
		case PTR:
			if(is_dynarray_ptr_type(dtype)) break;
			if(dtype->subtype->kind == ARRAY) write(")");
			gen_type_postfix(dtype->subtype);
			break;
		case ARRAY:
			write("[%u]%z", dtype->length, dtype->itemtype);
			break;
	}
}

static void gen_cast(Expr *expr)
{
	Type *dtype = expr->dtype;
	Type *subtype = dtype->subtype;
	Expr *srcexpr = expr->subexpr;
	Type *srctype = srcexpr->dtype;
	
	if(dtype->kind == BOOL) {
		write("(%e ? jatrue : jafalse)", srcexpr);
	}
	else if(is_dynarray_ptr_type(dtype)) {
		write("((%Y){.length = ", dtype);
		
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
	else {
		write("((%Y)%e)", dtype, srcexpr);
	}
}

static void gen_expr(Expr *expr)
{
	switch(expr->kind) {
		case INT:
			write("%i", expr->ival);
			break;
		case BOOL:
			write(expr->ival ? "jatrue" : "jafalse");
			break;
		case STRING:
			write(
				"((jastring){%i, \"%S\"})",
				expr->length, expr->string, expr->length
			);
			break;
		case VAR:
			write("%I", expr->id);
			break;
		case PTR:
			write("(&%e)", expr->subexpr);
			break;
		case DEREF:
			write("(*%e)", expr->subexpr);
			break;
		case CAST:
			gen_cast(expr);
			break;
		case SUBSCRIPT:
			if(
				expr->subexpr->kind == DEREF &&
				is_dynarray_ptr_type(expr->subexpr->subexpr->dtype)
			) {
				Expr *dynarray = expr->subexpr->subexpr;
				Expr *index = expr->index;
				Type *itemtype = expr->dtype;
				
				write(
					"(((%y(*)%z)%e.items)[%e])",
					itemtype, itemtype, dynarray, index
				);
			}
			else {
				write("(%e[%e])", expr->subexpr, expr->index);
			}
			break;
		case BINOP:
			write(
				"(%e %s %e)", expr->left, expr->operator->punct, expr->right
			);
			break;
		case ARRAY:
			write("((%Y){", expr->dtype);
			for(Expr *item = expr->exprs; item; item = item->next) {
				if(item != expr->exprs)
					write(", ");
				gen_expr(item);
			}
			write("})");
			break;
		case CALL:
			write("(%e()", expr->callee);
			if(expr->callee->dtype->returntype->kind == ARRAY) {
				write(".a");
			}
			write(")");
			break;
		case MEMBER:
			if(
				expr->subexpr->dtype->kind == ARRAY &&
				token_text_equals(expr->member_id, "length")
			) {
				if(
					expr->subexpr->kind == DEREF &&
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
	if(expr->kind == ARRAY) {
		write("{");
		for(Expr *item = expr->exprs; item; item = item->next) {
			if(item != expr->exprs)
				write(", ");
			gen_init_expr(item);
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

static void gen_assign(Expr *target, Expr *expr)
{
	if(target->dtype->kind == ARRAY) {
		if(expr->kind == ARRAY) {
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

static void gen_stmt(Stmt *stmt)
{
	switch(stmt->kind) {
		case PRINT:
			gen_print(stmt->expr);
			break;
		case VAR:
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
		case IF:
			write("%>if(%e) {\n", stmt->expr);
			gen_stmts(stmt->if_body);
			write("%>}\n");
			if(stmt->else_body) {
				write("%>else {\n");
				gen_stmts(stmt->else_body);
				write("%>}\n");
			}
			break;
		case WHILE:
			write("%>while(%e) {\n", stmt->expr);
			gen_stmts(stmt->while_body);
			write("%>}\n");
			break;
		case ASSIGN:
			gen_assign(stmt->target, stmt->expr);
			break;
		case CALL:
			write("%>%e;\n", stmt->call);
			break;
		case RETURN:
			if(stmt->expr) {
				Expr *result = stmt->expr;
				Type *dtype = result->dtype;
				if(dtype->kind == ARRAY) {
					if(result->kind == ARRAY) {
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
		case IMPORT:
			write("%>_%s_main(argc, argv);\n", stmt->unit->unit_id);
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

static void gen_export_alias(Token *ident, Unit *unit)
{
	write("#define %I _%s_%t\n", ident, unit->unit_id, ident);
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
	if(stmt->exported) {
		gen_export_alias(stmt->id, cur_unit);
	}
	
	write("%>");
	
	if(!stmt->scope->parent && !stmt->scope->struc && !stmt->exported)
		write("static ");
	
	write("%y %I%z", stmt->dtype, stmt->id, stmt->dtype);
	
	if(stmt->scope->struc) {
		write(";\n");
	}
	else if(stmt->expr) {
		// has initializer
		if(
			stmt->expr->isconst ||
			stmt->scope->parent && stmt->expr->kind != ARRAY
		) {
			// is constant or for local var (no array literal)
			// => in-place init possible
			write(" = ");
			gen_init_expr(stmt->expr);
			write(";\n");
		}
		else if(stmt->scope->parent && stmt->expr->kind == ARRAY) {
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
		stmt->dtype->kind == ARRAY || stmt->dtype->kind == STRUCT ||
		stmt->dtype->kind == STRING ||
		is_dynarray_ptr_type(stmt->dtype)
	) {
		write(" = {0};\n");
	}
	else {
		write(" = 0;\n");
	}
}

static void gen_func_returntype_decl(Stmt *func, int in_header)
{
	Type *returntype = func->dtype;
	if(returntype->kind == ARRAY) {
		if(func->exported) {
			if(in_header) {
				write(
					"%>typedef struct { %y a%z; } rt%X;\n",
					returntype, returntype, func->id
				);
			}
			else {
				write("#define rt%I rt%X\n", func->id, func->id);
				write(
					"%>typedef struct { %y a%z; } rt%I;\n",
					returntype, returntype, func->id
				);
			}
		}
		else {
			write(
				"%>typedef struct { %y a%z; } rt%I;\n",
				returntype, returntype, func->id
			);
		}
	}
}

static void gen_func_head(Stmt *stmt, int in_header)
{
	Type *returntype = stmt->dtype;
	char *static_prefix = stmt->exported ? "" : "static ";
	
	if(returntype->kind == ARRAY) {
		gen_func_returntype_decl(stmt, in_header);
		if(stmt->exported) {
			if(in_header) {
				write("%>rt%X %X()", stmt->id, stmt->id);
			}
			else {
				write("%>rt%I %I()", stmt->id, stmt->id);
			}
		}
		else {
			write("%>static rt%I %I()", stmt->id, stmt->id);
		}
	}
	else {
		if(stmt->exported) {
			if(in_header) {
				write("%>%y %X()%z", returntype, stmt->id, returntype);
			}
			else {
				write("%>%y %I()%z", returntype, stmt->id, returntype);
			}
		}
		else {
			write("%>static %y %I()%z", returntype, stmt->id, returntype);
		}
	}
}

static void gen_funcdecl(Stmt *stmt)
{
	if(stmt->exported) {
		gen_export_alias(stmt->id, cur_unit);
	}
	
	gen_func_head(stmt, 0);
	write("%>{\n");
	gen_stmts(stmt->func_body);
	write("%>}\n");
}

static void gen_structdecls(Stmt *stmts)
{
	for(Stmt *stmt = stmts; stmt; stmt = stmt->next) {
		if(stmt->kind == STRUCT) {
			gen_structdecl(stmt);
		}
	}
}

static void gen_vardecls(Stmt *stmts)
{
	for(Stmt *stmt = stmts; stmt; stmt = stmt->next) {
		if(stmt->kind == VAR) {
			gen_vardecl(stmt);
		}
	}
}

static void gen_funcdecls(Stmt *stmts)
{
	for(Stmt *stmt = stmts; stmt; stmt = stmt->next) {
		if(stmt->kind == FUNC) {
			gen_funcdecl(stmt);
		}
	}
}

static void gen_imports(Stmt *stmts)
{
	for(Stmt *stmt = stmts; stmt; stmt = stmt->next) {
		if(stmt->kind == IMPORT) {
			write("#include \"%s\"\n", stmt->unit->h_filename);
			
			for(int64_t i=0; i < stmt->imported_ident_count; i++) {
				Token *ident = stmt->imported_idents + i * 2;
				gen_export_alias(ident, stmt->unit);
			}
		}
	}
}

static void gen_h()
{
	ofs = fopen(cur_unit->h_filename, "wb");
	write("#include <stdint.h>\n");
	write("int _%s_main(int argc, char **argv);\n", cur_unit->unit_id);
	
	for(Stmt *stmt = cur_unit->stmts; stmt; stmt = stmt->next) {
		if(stmt->kind == FUNC && stmt->exported) {
			gen_func_head(stmt, 1);
			write(";\n");
		}
	}
	
	for(Stmt *stmt = cur_unit->stmts; stmt; stmt = stmt->next) {
		if(stmt->kind == VAR && stmt->exported) {
			write("extern %y %X%z;\n", stmt->dtype, stmt->id, stmt->dtype);
		}
	}
	
	fclose(ofs);
}

static void gen_c()
{
	ofs = fopen(cur_unit->c_filename, "wb");
	level = 0;
	
	write(RUNTIME_H_SRC);
	
	gen_imports(cur_unit->stmts);
	gen_structdecls(cur_unit->stmts);
	gen_vardecls(cur_unit->stmts);
	gen_funcdecls(cur_unit->stmts);
	
	write("static jadynarray ja_argv;\n");
	write("static int main_was_called;\n");
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
