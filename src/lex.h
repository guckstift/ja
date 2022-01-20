#ifndef LEX_H
#define LEX_H

#include <stdint.h>

#define KEYWORDS(_) \
	_(as) \
	_(bool) \
	_(else) \
	_(false) \
	_(if) \
	_(int) \
	_(print) \
	_(true) \
	_(uint) \
	_(var) \
	_(while) \

#define PUNCTS(_) \
	_("{", LCURLY) \
	_("}", RCURLY) \
	_("<", LOWER) \
	_(">", GREATER) \
	_("=", ASSIGN) \
	_(":", COLON) \
	_(";", SEMICOLON) \
	_("+", PLUS) \
	_("-", MINUS) \

typedef enum {
	TK_EOF,
	TK_IDENT,
	TK_INT,
	
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
	char *linep;
	char *start;
	int64_t length;
	union {
		int64_t ival;
		uint64_t uval;
		struct Token *id;
		char *punct;
	};
	struct Token *next_id;
} Token;

typedef struct {
	Token *first;
	Token *last;
} Tokens;

char *get_token_type_name(TokenType type);
Tokens *lex(char *src, int64_t src_len);

#endif
