#ifndef ERROR_H
#define ERROR_H

#include "ast.h"
#include "print.h"

/*
#define ERRORS \
	_(unterminated_ml_comment, "unterminated multi line comment") \
	_(unrecognized_token, "unrecognized token %i") \
	_(vardecl_no_type_no_init, "variable declaration has neither a type nor an initializer") \
	_(already_declared, "name %t already declared") \
	_(no_valid_stmt, "can not find a valid statement") \
	_(expected, "expected %s") \

#define EXPECTS \
	_(expr, "expression") \
	_(type, "type") \
*/

#define error_at_tok(t, ...)     (error_tok = t, error(__VA_ARGS__))
#define error_after_tok(t, ...)  (error_after = 1, error_at(t, __VA_ARGS__))

#define error_at(a, ...) \
	_Generic(a, \
		Token*: error_at_tok((Token*)a, __VA_ARGS__), \
		Expr*: error_at_tok(((Expr*)a)->start, __VA_ARGS__), \
		Stmt*: error_at_tok(((Stmt*)a)->start, __VA_ARGS__) \
	)

typedef struct ErrorRecord {
	char *msg;
	int isfatal;
	Token tok;
	int after;
	struct ErrorRecord *next;
	ArgLog arglog;
} ErrorRecord;

extern Token *error_tok;
extern int    error_after;

extern ErrorRecord *error_first;
extern ErrorRecord *error_last;

/*
#define _(x, y) extern char *ERROR_ ## x;
ERRORS
#undef _

#define _(x, y) extern char *EXPECT_ ## x;
EXPECTS
#undef _
*/

void error(char *msg, ...);
void error_reset_opts();
void error_reset_records();
int had_errors();

#endif