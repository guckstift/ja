#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "lex.h"

char *get_token_type_name(TokenType type)
{
	switch(type) {
		case TK_EOF:
			return "end of file";
		case TK_IDENT:
			return "identifier";
		case TK_INT:
			return "integer literal";
		
		#define F(x) \
			case TK_ ## x: \
				return "keyword " #x;
		
		KEYWORDS(F)
		#undef F
		
		#define F(x, y) \
			case TK_ ## y: \
				return "'" x "'";
		
		PUNCTS(F)
		#undef F
	}
}

Token *lex(char *src)
{
	Token *tokens = 0;
	int64_t token_count = 0;
	Token *last_token = 0;
	char *pos = src;
	int64_t line = 1;
	char *linep = pos;
	
	#define emit(t) do { \
		token_count ++; \
		tokens = realloc(tokens, sizeof(Token) * token_count); \
		last_token = tokens + token_count - 1; \
		last_token->type = (t); \
		last_token->line = line; \
		last_token->start = start; \
		last_token->length = pos - start; \
	} while(0)
	
	#define match(s) ( \
		strlen(s) == 1 ? (s)[0] == pos[0] : \
		strlen(s) == 2 ? (s)[0] == pos[0] && (s)[1] == pos[1] : \
	0)
	
	#define hex2int(x) ( \
		(x) >= '0' && (x) <= '9' ? x - '0' : \
		(x) >= 'a' && (x) <= 'f' ? x - 'a' + 10 : \
		(x) >= 'A' && (x) <= 'F' ? x - 'A' + 10 : \
	0)
	
	#define tokequ(a, b) ( \
		(a)->length == (b)->length && \
		memcmp((a)->start, (b)->start, (a)->length) == 0 \
	)
	
	while(*pos) {
		char *start = pos;
	
		// whitespace
		
		if(isspace(*pos)) {
			if(*pos == '\n') {
				pos ++;
				line ++;
				linep = pos;
			}
			else {
				pos ++;
			}
		}
		
		// comments
		
		else if(*pos == '#') {
			while(*pos && *pos != '\n') pos ++;
		}
		else if(match("/*")) {
			pos += 2;
			while(*pos) {
				if(match("*/")) {
					pos += 2;
					break;
				}
				else if(*pos == '\n') {
					pos ++;
					line ++;
					linep = pos;
				}
				else {
					pos ++;
				}
			}
		}
		
		// identifiers / keywords
		
		else if(isalpha(*pos)) {
			while(isalnum(*pos)) pos ++;
			emit(TK_IDENT);
			
			#define F(x) \
				if( \
					last_token->length == strlen(#x) && \
					memcmp(last_token->start, #x, strlen(#x)) == 0 \
				) { \
					last_token->type = TK_ ## x; \
				} else
			
			KEYWORDS(F);
			#undef F
		}
		
		// numbers
		
		else if(isdigit(*pos)) {
			int64_t ival = 0;
			
			if(match("0x")) {
				pos += 2;
				while(isxdigit(*pos)) {
					ival *= 16;
					ival += hex2int(*pos);
					pos ++;
				}
				emit(TK_INT);
				last_token->ival = ival;
			}
			else {
				while(isdigit(*pos)) {
					ival *= 10;
					ival += *pos - '0';
					pos ++;
				}
				if(*pos == '.') {
					pos ++;
					while(isdigit(*pos)) pos ++;
					int64_t len = pos - start;
					char buf[len + 1];
					buf[len] = 0;
					memcpy(buf, start, len);
					double fval = strtod(buf, 0);
					emit(TK_FLOAT);
					last_token->fval = fval;
				}
				else {
					emit(TK_INT);
					last_token->ival = ival;
				}
			}
		}
		
		// punctuators
		
		#define F(x, y) \
			else if(match(x)) { \
				pos += strlen(x); \
				emit(TK_ ## y); \
			}
		
		PUNCTS(F)
		#undef F
		
		// error
		
		else {
			fprintf(stderr, "error(%" PRId64 "): unrecognized token\n", line);
			exit(EXIT_FAILURE);
		}
	}
	
	char *start = pos;
	emit(TK_EOF);
	
	Token *first_id = 0;
	Token *last_id = 0;
	
	for(Token *token = tokens; token->type != TK_EOF; token ++) {
		if(token->type == TK_IDENT) {
			token->id = 0;
			token->next_id = 0;
			for(Token *id = first_id; id; id = id->next_id) {
				if(tokequ(token, id)) {
					token->id = id;
					break;
				}
			}
			if(token->id == 0) {
				token->id = token;
				if(last_id) last_id = last_id->next_id = token;
				else first_id = last_id = token;
			}
		}
	}
	
	return tokens;
}
