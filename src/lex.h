#ifndef LEX_H
#define LEX_H

#include <stdint.h>
#include <string.h>

#define KEYWORDS(_) \
	_(as) \
	_(bool) \
	_(break) \
	_(continue) \
	_(cstring) \
	_(delete) \
	_(else) \
	_(enum) \
	_(export) \
	_(false) \
	_(for) \
	_(foreign) \
	_(from) \
	_(function) \
	_(if) \
	_(import) \
	_(in) \
	_(int) \
	_(int8) \
	_(int16) \
	_(int32) \
	_(int64) \
	_(new) \
	_(print) \
	_(ptr) \
	_(string) \
	_(struct) \
	_(return) \
	_(true) \
	_(uint) \
	_(uint8) \
	_(uint16) \
	_(uint32) \
	_(uint64) \
	_(union) \
	_(var) \
	_(while) \

#define PUNCTS(_) \
	_("..", DOTDOT) \
	_("&&", AND) \
	_("||", OR) \
	_("==", EQUALS) \
	_("!=", NEQUALS) \
	_("<=", LEQUALS) \
	_(">=", GEQUALS) \
	_("//", DSLASH) \
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
	_("*", MUL) \
	_("%", MOD) \
	_("&", AMP) \
	_("|", PIPE) \
	_("^", XOR) \
	_("~", TILDE) \

#define tokequ(a, b) ( \
	(a)->length == (b)->length && \
	memcmp((a)->start, (b)->start, (a)->length) == 0 \
)

#define tokequ_str(t, s) ( \
	(t)->length == strlen(s) && \
	memcmp((t)->start, (s), (t)->length) == 0 \
)

typedef enum {
	#define F(x) KW_ ## x,
	KEYWORDS(F)
	#undef F
} Keyword;

typedef enum {
	#define F(x,y) PT_ ## y,
	PUNCTS(F)
	#undef F
} Punct;

typedef enum {
	TK_EOF,
	TK_IDENT,
	TK_INT,
	TK_STRING,
	TK_KEYWORD,
	TK_PUNCT,
} TokenKind;

typedef struct Token {
	TokenKind kind;
	int64_t line;
	char *linep;
	char *start;
	int64_t length;

	union {
		int64_t ival;
		int64_t string_length;
		Keyword keyword;
		Punct punct_id;
	};

	union {
		struct Token *id;
		char *punct;
		char *string;
	};
} Token;

Token *create_id(char *start, int64_t length);
Token *lex(char *src, long length);

#endif
