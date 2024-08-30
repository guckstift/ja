#ifndef LEX_H
#define LEX_H

#include <stdint.h>
#include "ast.h"

Token *mock_id();
void lex(Module *module);
void lex_reset();

#endif