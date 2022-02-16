#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include "cgen.h"
#include "runtime.inc.h"
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
static void gen_assign(Expr *target, Expr *expr);
static void gen_type(Type *dtype);
static void gen_type_postfix(Type *dtype);
static void gen_expr(Expr *expr);
static void gen_init_expr(Expr *expr);
static void gen_stmt(Stmt *stmt, int noindent);
static void gen_exprs(Expr **exprs);

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

static void gen_struct_type(Type *dtype)
{
	if(dtype->typedecl->exported && in_header) {
		write("%X", dtype->id);
	}
	else if(
		dtype->typedecl->scope != cur_unit->stmts[0]->scope &&
		dtype->typedecl->imported == 0
	) {
		write("%s", dtype->typedecl->public_id);
	}
	else {
		write("%I", dtype->id);
	}
}

static void gen_ptr_type(Type *dtype)
{
	if(is_dynarray_ptr_type(dtype)) {
		write("jadynarray");
	}
	else {
		gen_type(dtype->subtype);
		if(dtype->subtype->kind == ARRAY) write("(");
		write("*");
	}
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
			gen_struct_type(dtype);
			break;
		case PTR:
			gen_ptr_type(dtype);
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
}

static void gen_binop(Expr *expr)
{
	if(
		expr->left->dtype->kind == STRING &&
		expr->operator->type == TK_EQUALS
	) {
		write(
			"(%e.length == %e.length && "
			"memcmp(%e.string, %e.string, %e.length) == 0)",
			expr->left, expr->right,
			expr->left, expr->right, expr->left
		);
	}
	else {
		write("(%e %s %e)", expr->left, expr->operator->punct, expr->right);
	}
}

static void gen_array(Expr *expr)
{
	write("((%Y){", expr->dtype);
	gen_exprs(expr->exprs);
	write("})");
}

static void gen_call(Expr *expr)
{
	write("(%e(", expr->callee);
	gen_exprs(expr->args);
	write(")");
	
	if(expr->callee->dtype->returntype->kind == ARRAY) {
		write(".a");
	}
	
	write(")");
}

static void gen_member(Expr *expr)
{
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
			gen_string(expr);
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
	}
}

static void gen_exprs(Expr **exprs)
{
	array_for(exprs, i) {
		if(i > 0) write(", ");
		gen_expr(exprs[i]);
	}
}

static void gen_init_expr(Expr *expr)
{
	if(expr->kind == ARRAY) {
		write("{");
		array_for(expr->exprs, i) {
			if(i > 0) write(", ");
			gen_init_expr(expr->exprs[i]);
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
				new_var_expr(decl->id, decl->dtype, decl->start),
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
				new_var_expr(decl->id, decl->dtype, decl->start), decl->init
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
	write(RUNTIME_H_SRC);
	
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
	write(RUNTIME_H_SRC);
	
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
