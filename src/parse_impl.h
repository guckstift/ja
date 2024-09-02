#ifndef PARSE_IMPL_H
#define PARSE_IMPL_H

#include <stdbool.h>
#include "ast.h"
#include "parse.h"
#include "error.h"

#define enter        parse_enter
#define leave        parse_leave
#define setscope     parse_setscope
#define setcur       parse_setcur
#define peek         parse_peek
#define advance      parse_advance
#define getlast      parse_last
#define expect       parse_expect

#define error_at_cur(...)  error_at(peek(), __VA_ARGS__)

#define match(k)  (peek()->kind == k)
#define eat(k)    (match(k) ? advance() : 0)

extern int e_before;

void enter();
Scope *leave();
Scope *touchscope();
void setscope(Scope *new_scope);
bool declare(Stmt *decl);
void *setcur(Token *pos);
Token *peek();
Token *advance();
Token *getlast();
void *expect(Kind kind, char *msg);

#endif