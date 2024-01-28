#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "lex.h"
#include "print.h"
#include "utils/array.h"

static char *pos;
static char *end;
static char *start;
static char *line_start;
static int64_t line_num;
static Token *tokens;
static Token *last;
static Token **ids;

static inline uint8_t hex_char_to_int(char x)
{
	return
		"\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0"
		"\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0"
		"\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0"
		"\x0\x1\x2\x3\x4\x5\x6\x7\x8\x9\x0\x0\x0\x0\x0\x0"
		"\x0\xA\xB\xC\xD\xE\xF\x0\x0\x0\x0\x0\x0\x0\x0\x0"
		"\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0"
		"\x0\xa\xb\xc\xd\xe\xf\x0\x0\x0\x0\x0\x0\x0\x0\x0"
		"\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0"
		[x & 0x7f]
	;
}

static inline void emit(TokenKind kind)
{
	Token token = {.kind = kind, .line = line_num, .linep = line_start, .start = start, .length = pos - start};
	array_push(tokens, token);
	last = tokens + array_length(tokens) - 1;
}

static inline int match(char *str)
{
	for(long i=0; i < strlen(str); i++) {
		if(str[i] != pos[i]) {
			return 0;
		}
	}

	return 1;
}

static inline int token_equal(Token *a, Token *b)
{
	return a->length == b->length && memcmp(a->start, b->start, a->length) == 0;
}

Token *lex(char *src, long length)
{
	pos = src;
	end = src + length;
	line_start = src;
	line_num = 1;
	tokens = 0;
	last = 0;

	while(pos < end) {
		start = pos;

		if(*pos == '\n') {
			pos ++;
			line_num ++;
			line_start = pos;
		}

		else if(*pos == ' ' || *pos == '\t') {
			pos ++;
		}

		else if(*pos == '#') {
			while(*pos != 0 && *pos != '\n') {
				pos ++;
			}
		}

		else if(match("/*")) {
			int64_t first_line_num = line_num;
			char *first_line_start = line_start;
			pos += 2;

			while(pos < end) {
				if(match("*/")) {
					pos += 2;
					break;
				}
				else if(*pos == '\n') {
					pos ++;
					line_num ++;
					line_start = pos;
				}
				else {
					pos ++;
				}
			}

			if(pos == end) {
				print_error(
					first_line_num, first_line_start, end, first_line_start,
					"unterminated multi line comment"
				);

				exit(EXIT_FAILURE);
			}
		}

		else if(isalpha(*pos) || *pos == '_') {
			while(isalnum(*pos) || *pos == '_') {
				pos ++;
			}

			emit(TK_IDENT);

			#define F(x) \
				if(last->length == strlen(#x) && memcmp(last->start, #x, strlen(#x)) == 0) { \
					last->kind = TK_KEYWORD; \
					last->keyword = KW_ ## x; \
					continue; \
				} \

			KEYWORDS(F);
			#undef F
		}

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
					ival += hex_char_to_int(*pos);
					pos ++;
				}

				emit(TK_INT);
				last->ival = ival;
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
				last->ival = ival;
			}
		}

		else if(*pos == '"') {
			int64_t first_line_num = line_num;
			char *first_line_start = line_start;
			pos ++;
			char *str_start = pos;

			while(pos < end && *pos != '"') {
				pos ++;
			}

			if(pos == end) {
				print_error(
					first_line_num, first_line_start, end, first_line_start,
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
			last->string = buf;
			last->string_length = length;
		}

		#define F(x, y) \
			else if(match(x)) { \
				pos += strlen(x); \
				emit(TK_PUNCT); \
				last->punct = x; \
				last->punct_id = PT_ ## y; \
			}

		PUNCTS(F)
		#undef F

		else if(ispunct(*pos)) {
			print_error(
				line_num, line_start, end, pos,
				"unrecognized punctuator '%c' (ignoring)",
				(uint8_t)*pos
			);

			pos ++;
		}

		else {
			print_error(
				line_num, line_start, end, pos,
				"unrecognized character (byte value: 0x%b; ignoring)",
				(uint8_t)*pos
			);

			pos ++;
		}
	}

	start = pos;
	emit(TK_EOF);

	for(Token *token = tokens; token->kind != TK_EOF; token ++) {
		if(token->kind == TK_IDENT) {
			token->id = 0;

			array_for(ids, i) {
				if(token_equal(token, ids[i])) {
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
		if(token_equal(ident, ids[i])) {
			free(ident);
			return ids[i];
		}
	}

	ident->id = ident;
	array_push(ids, ident);
	return ident;
}
