#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include "lex.h"
#include "array.h"
#include "error.h"
#include "arena.h"

static Token **idtab;
static uint64_t mock_id_counter;

Token *mock_id()
{
	char buf[32];
	sprintf(buf, "%lu", mock_id_counter);
	mock_id_counter ++;
	int64_t len = strlen(buf);
	char *str = alloc(len + 1);
	strcpy(str, buf);
	Token *token = create_token(TK_IDENT, .start = str, .length = len);
	token->id = token;
	return token;
}

static int ident_equal(Token *a, Token *b)
{
	return a->length == b->length && memcmp(a->start, b->start, a->length) == 0;
}

static Token *intern_ident(Token *ident)
{
	array_for(idtab, i)
		if(ident_equal(idtab[i], ident))
			return idtab[i];

	array_push(idtab, ident);
	return idtab[array_length(idtab) - 1];
}

static Line *create_lines(char *src)
{
	Line *lines = 0;
	Line line = {.index = 0, .start = src};

	while(*src) {
		if(*src == '\n') {
			line.end = src;
			array_push(lines, line);
			line.index ++;
			line.start = src + 1;
		}

		src ++;
	}

	line.end = src;
	array_push(lines, line);
	return lines;
}

void lex(Module *module)
{
	char *src = module->src;
	Line *lines = create_lines(src);
	Token *tokens = 0;
	Token token = {.start = src, .line = lines, .index = 0};

	while(*src) {
		token.start = src;

		// new line

		 if(*src == '\n') {
			src ++;
			token.line ++;
			continue;
		}

		// whitespace

		else if(*src == ' ' || *src == '\t') {
			while(*src == ' ' || *src == '\t')
				src ++;

			continue;
		}

		// comments

		else if(*src == '#') {
			while(*src && *src != '\n')
				src ++;

			continue;
		}

		else if(src[0] == '/' && src[1] == '*') {
			Line *line = token.line;
			src += 2;

			while(*src) {
				if(src[0] == '*' && src[1] == '/') {
					break;
				}
				else if(*src == '\n') {
					src ++;
					line ++;
				}
				else {
					src ++;
				}
			}

			if(src[0] == '*' && src[1] == '/')
				src += 2;
			else
				error_at(&token, "unterminated multi line comment");

			token.line = line;
			continue;
		}

		// integers

		else if(isdigit(*src)) {
			token.ival = 0;

			while(isdigit(*src)) {
				token.ival = token.ival * 10 + *src - '0';
				src ++;
			}

			token.kind = TK_INT;
		}

		// idents / keywords

		else if(isalpha(*src) || *src == '_') {
			while(isalnum(*src) || *src == '_')
				src ++;

			token.kind = TK_IDENT;
			token.length = src - token.start;

			#define _(x) \
				if(token.length == strlen(#x) && memcmp(token.start, #x, strlen(#x)) == 0) \
					token.kind = KW_ ## x; \
				else
			KEYWORDS;
			#undef _
		}

		// puncts

		#define _(x, y) \
			else if(memcmp(token.start, x, strlen(x)) == 0) { \
				token.kind = PT_ ## y; \
				src += strlen(x); \
			}
		PUNCTS
		#undef _

		// invalid

		else {
			token.length = 1;
			error_at(&token, "unrecognized token %i", *src);
			src ++;
			continue;
		}

		token.length = src - token.start;
		array_push(tokens, token);
		token.index ++;
	}

	// end of file

	token.start = src;
	token.kind = TK_EOF;
	array_push(tokens, token);
	token.index ++;

	// setting first-in-line tokens and interning identifiers

	array_for(tokens, i) {
		Token *token = tokens + i;

		if(token->line->first == 0)
			token->line->first = token;

		if(token->kind == TK_IDENT)
			token->id = intern_ident(token);
	}

	module->lines = lines;
	module->tokens = tokens;
}

void lex_reset()
{
	idtab = 0;
	mock_id_counter = 0;
}