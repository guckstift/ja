#include <stdlib.h>
#include "print.h"

#define adv()       (last = cur++)
#define match(t)    (cur->kind == (t))
#define matchkw(k)  (cur->kind == TK_KEYWORD && cur->keyword == (k))
#define matchpt(p)  (cur->kind == TK_PUNCT && cur->punct_id == (p))
#define eat(t)      (match(t) ? adv() : 0)
#define eatkw(k)    (matchkw(k) ? adv() : 0)
#define eatpt(p)    (matchpt(p) ? adv() : 0)

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

#define PARSER_NAME(a, ...)  p_ ## a

#define p_(name, ...) \
	( p_ ## name(__VA_ARGS__) )
//	( printf("entering " #name "\n"), p_ ## name(__VA_ARGS__) )

typedef struct {
	Token *cur;
	Token *last;
	char *src_end;
	Scope *scope;
	char *unit_id;
} ParseState;

extern Token *cur;
extern Token *last;
extern char *src_end;
extern Scope *scope;
extern char *unit_id;

Block *p_block(Scope *scope);
Expr *p_expr();
Type *p_type();

Decl *lookup_flat(Token *id);
Decl *lookup(Token *id);
void unpack_state(ParseState *state);
void pack_state(ParseState *state);