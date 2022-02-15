#include <stdlib.h>
#include "print.h"

#define match(t) (cur->type == (t))
#define match2(t1, t2) (cur[0].type == (t1) && cur[1].type == (t2))
#define adv() (last = cur++)
#define eat(t) (match(t) ? adv() : 0)
#define eat2(t1, t2) (match2(t1, t2) ? (adv(), adv()) : 0)

#define error(line, linep, start, ...) \
	print_error(line, linep, src_end, start, __VA_ARGS__)

#define error_at(token, ...) \
	error((token)->line, (token)->linep, (token)->start, __VA_ARGS__)

#define error_after(token, ...) \
	error( \
		(token)->line, (token)->linep, (token)->start + (token)->length, \
		__VA_ARGS__ \
	)

#define fatal(line, linep, start, ...) do { \
	error(line, linep, start, __VA_ARGS__); \
	exit(EXIT_FAILURE); \
} while(0)

#define fatal_at(token, ...) \
	fatal((token)->line, (token)->linep, (token)->start, __VA_ARGS__)

#define fatal_after(token, ...) \
	fatal( \
		(token)->line, (token)->linep, (token)->start + (token)->length, \
		__VA_ARGS__ \
	)

typedef struct {
	Token *cur;
	Token *last;
	char *src_end;
	Scope *scope;
	char *unit_id;
} ParseState;

Expr *cast_expr(Expr *expr, Type *dtype, int explicit);
Expr *p_expr_pub(ParseState *state);

Stmt *p_stmt_pub(ParseState *state);
Stmt *p_stmts_pub(ParseState *state, Decl *func);

void make_type_exportable(Type *dtype);
Type *complete_type(Type *dtype, Expr *expr);
Type *p_type_pub(ParseState *state);

static Token *cur;
static Token *last;
static char *src_end;
static Scope *scope;
static char *unit_id;

static Decl *lookup_builtin(Token *id)
{
	if(scope->parent) return 0;
	
	if(token_text_equals(id, "argv")) {
		static Decl *argv = 0;
		if(argv == 0) {
			argv = new_vardecl(
				id, new_ptr_type(new_array_type(-1, new_type(STRING))),
				0, 0, scope
			);
		}
		return argv;
	}
	
	return 0;
}

static Decl *lookup_flat(Token *id)
{
	Decl *decl = lookup_builtin(id);
	if(decl) return decl;
	return lookup_flat_in(id, scope);
}

static Decl *lookup(Token *id)
{
	Decl *decl = lookup_builtin(id);
	if(decl) return decl;
	return lookup_in(id, scope);
}

static void unpack_state(ParseState *state)
{
	cur = state->cur;
	last = state->last;
	src_end = state->src_end;
	scope = state->scope;
	unit_id = state->unit_id;
}

static void pack_state(ParseState *state)
{
	state->cur = cur;
	state->last = last;
	state->src_end = src_end;
	state->scope = scope;
	state->unit_id = unit_id;
}
