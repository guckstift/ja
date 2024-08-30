#ifndef GEN_STMT_H
#define GEN_STMT_H

#include "ast.h"

void gen_vardecl(Stmt *decl);
void gen_stmt(Stmt *stmt);
void gen_stmts(Stmt **stmts);
void gen_block(Block *block);

#endif