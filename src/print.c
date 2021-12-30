#include <stdio.h>
#include <inttypes.h>
#include "lex.h"

void print_token(Token *token)
{
	printf("%" PRId64 ": ", token->line);
	
	switch(token->type) {
		case TK_IDENT:
			printf("IDENT    ");
			fwrite(token->start, 1, token->length, stdout);
			break;
		case TK_INT:
			printf("INT      %" PRId64, token->ival);
			break;
		case TK_FLOAT:
			printf("FLOAT   ");
			break;
		
		#define F(x) \
			case TK_ ## x: \
				printf("KEYWORD  "); \
				fwrite(token->start, 1, token->length, stdout); \
				break;
		
		KEYWORDS(F)
		#undef F
		
		#define F(x, y) \
			case TK_ ## y: \
				printf("PUNCT    " x); \
				break;
		
		PUNCTS(F)
		#undef F
	}
}

void print_tokens(Token *tokens)
{
	for(Token *token = tokens; token->type != TK_EOF; token ++) {
		print_token(token);
		printf("\n");
	}
}
