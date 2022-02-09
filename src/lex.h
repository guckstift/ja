#ifndef LEX_H
#define LEX_H

#include <stdint.h>

#define KEYWORDS(_) \
	_(as) \
	_(bool) \
	_(else) \
	_(false) \
	_(function) \
	_(if) \
	_(import) \
	_(int) \
	_(int8) \
	_(int16) \
	_(int32) \
	_(int64) \
	_(print) \
	_(string) \
	_(struct) \
	_(return) \
	_(true) \
	_(uint) \
	_(uint8) \
	_(uint16) \
	_(uint32) \
	_(uint64) \
	_(var) \
	_(while) \

#define PUNCTS(_) \
	_("{", LCURLY) \
	_("}", RCURLY) \
	_("<", LOWER) \
	_(">", GREATER) \
	_("[", LBRACK) \
	_("]", RBRACK) \
	_("(", LPAREN) \
	_(")", RPAREN) \
	_("=", ASSIGN) \
	_(".", PERIOD) \
	_(":", COLON) \
	_(";", SEMICOLON) \
	_(",", COMMA) \
	_("+", PLUS) \
	_("-", MINUS) \

typedef enum {
	TK_EOF,
	TK_IDENT,
	TK_INT,
	TK_STRING,
	
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
		char *string;
	};
	union {
		struct Token *next_id;
		int64_t string_length;
	};
} Token;

typedef struct {
	Token *first;
	Token *last;
} Tokens;

char *get_token_type_name(TokenType type);
int token_text_equals(Token *token, char *text);
Tokens *lex(char *src, int64_t src_len);

#endif
