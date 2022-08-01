#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "lex.h"
#include "print.h"
#include "array.h"

#define hex2int(x) ( \
	(x) >= '0' && (x) <= '9' ? x - '0' : \
	(x) >= 'a' && (x) <= 'f' ? x - 'a' + 10 : \
	(x) >= 'A' && (x) <= 'F' ? x - 'A' + 10 : \
0)

#define tokequ(a, b) ( \
	(a)->length == (b)->length && \
	memcmp((a)->start, (b)->start, (a)->length) == 0 \
)

static Token **ids = 0;

char *get_token_kind_name(TokenKind kind)
{
	switch(kind) {
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

int token_text_equals(Token *token, char *text)
{
	return
		strlen(text) == token->length &&
		memcmp(text, token->start, token->length) == 0 ;
}

Token *create_id(char *start, int64_t length)
{
	if(length == 0) {
		length = strlen(start);
	}
	
	Token *ident = malloc(sizeof(Token));
	ident->kind = TK_IDENT;
	ident->line = 0;
	ident->linep = 0;
	ident->start = start;
	ident->length = length;
	
	array_for(ids, i) {
		if(tokequ(ident, ids[i])) {
			free(ident);
			return ids[i];
		}
	}
	
	ident->id = ident;
	array_push(ids, ident);
	return ident;
}

Token *lex(char *src, int64_t src_len)
{
	char *src_end = src + src_len;
	Token *tokens = 0;
	Token *last_token = 0;
	char *pos = src;
	int64_t line = 1;
	char *linep = pos;
	
	#define emit(t) do { \
		array_push(tokens, ((Token){ \
			.kind = (t), \
			.line = line, \
			.linep = linep, \
			.start = start, \
			.length = pos - start, \
		})); \
		last_token = tokens + array_length(tokens) - 1; \
	} while(0)
	
	#define match(s) ( \
		strlen(s) == 1 ? (s)[0] == pos[0] : \
		strlen(s) == 2 ? (s)[0] == pos[0] && (s)[1] == pos[1] : \
	0)
	
	while(pos < src_end) {
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
			while(pos < src_end && *pos != '\n') pos ++;
		}
		else if(match("/*")) {
			int64_t start_line = line;
			char *start_linep = linep;
			pos += 2;
			while(pos < src_end) {
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
			if(pos == src_end) {
				print_error(
					start_line, start_linep, src_end, start_linep,
					"unterminated multi line comment"
				);
				exit(EXIT_FAILURE);
			}
		}
		
		// identifiers / keywords
		
		else if(isalpha(*pos) || *pos == '_') {
			while(isalnum(*pos) || *pos == '_') pos ++;
			emit(TK_IDENT);
			
			#define F(x) \
				if( \
					last_token->length == strlen(#x) && \
					memcmp(last_token->start, #x, strlen(#x)) == 0 \
				) { \
					last_token->kind = TK_ ## x; \
				} else
			
			KEYWORDS(F);
			#undef F
		}
		
		// numbers
		
		else if(isdigit(*pos)) {
			int64_t ival = 0;
			
			if(match("0x")) {
				pos += 2;
				while(isxdigit(*pos) || *pos == '_') {
					if(*pos == '_') {
						pos ++;
						continue;
					}
					ival *= 16;
					ival += hex2int(*pos);
					pos ++;
				}
				emit(TK_INT);
				last_token->ival = ival;
			}
			else {
				while(isdigit(*pos) || *pos == '_') {
					if(*pos == '_') {
						pos ++;
						continue;
					}
					ival *= 10;
					ival += *pos - '0';
					pos ++;
				}
				emit(TK_INT);
				last_token->ival = ival;
			}
		}
		
		// strings
		
		else if(*pos == '"') {
			int64_t start_line = line;
			char *start_linep = linep;
			pos ++;
			char *str_start = pos;
			while(pos < src_end && *pos != '"') pos ++;
			
			if(pos == src_end) {
				print_error(
					start_line, start_linep, src_end, start_linep,
					"unterminated string literal"
				);
				exit(EXIT_FAILURE);
			}
			
			int64_t length = pos - str_start;
			char *buf = malloc(length + 1);
			memcpy(buf, str_start, length);
			buf[length] = 0;
			pos ++;
			emit(TK_STRING);
			last_token->string = buf;
			last_token->string_length = length;
		}
		
		// punctuators
		
		#define F(x, y) \
			else if(match(x)) { \
				pos += strlen(x); \
				emit(TK_ ## y); \
				last_token->punct = x; \
			}
		
		PUNCTS(F)
		#undef F
		
		// unrecognized punctuator
		
		else if(ispunct(*pos)) {
			print_error(
				line, linep, src_end, pos,
				"unrecognized punctuator '%c' (ignoring)",
				(uint8_t)*pos
			);
			pos ++;
		}
		
		// unrecognized character
		
		else {
			print_error(
				line, linep, src_end, pos,
				"unrecognized character (byte value: 0x%b; ignoring)",
				(uint8_t)*pos
			);
			pos ++;
		}
	}
	
	char *start = pos;
	emit(TK_EOF);
	
	for(Token *token = tokens; token->kind != TK_EOF; token ++) {
		if(token->kind == TK_IDENT) {
			token->id = 0;
			array_for(ids, i) {
				if(tokequ(token, ids[i])) {
					token->id = ids[i];
					break;
				}
			}
			if(token->id == 0) {
				token->id = token;
				array_push(ids, token);
			}
		}
	}
	
	return tokens;
}
