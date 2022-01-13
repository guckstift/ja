#ifndef LEX_H
#define LEX_H

#include <stdint.h>

#define KEYWORDS(_) \
	_(print) \
	_(var) \

#define PUNCTS(_) \
	_("=", ASSIGN) \
	_(";", SEMICOLON) \
	_(",", COMMA) \

typedef enum {
	TK_EOF,
	TK_IDENT,
	TK_INT,
	TK_FLOAT,
	
	#define F(x) TK_ ## x,
	KEYWORDS(F)
	#undef F
	
	#define F(x, y) TK_ ## y,
	PUNCTS(F)
	#undef F

} TokenType;

typedef struct Token {
	TokenType type;
	int64_t line;
	char *start;
	int64_t length;
	union {
		int64_t ival;
		double fval;
		struct Token *id;
	};
	struct Token *next_id;
} Token;

char *get_token_type_name(TokenType type);
Token *lex(char *src);

#endif
