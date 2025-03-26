#ifndef lex_H
#define lex_H

#ifndef IMPLEMENT_FLAG
#define IMPLEMENT_FLAG
#define lex_C
#endif

#include "ast.c"

void lex(Module *module);

#endif
#ifdef lex_C

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "array.c"
#include "error.c"
#include "arena.c"
#include "table.c"
#include "string.c"

#define token_equals_strlit(t, s) ((t).length == sizeof(s)-1 && memcmp((t).start, (s), sizeof(s)-1) == 0)

#define hexchar_to_int(h) ( \
	(h) >= '0' && (h) <= '9' ? (h) - '0' : \
	(h) >= 'A' && (h) <= 'F' ? (h) - 'A' + 10 : \
	(h) - 'a' + 10 \
)

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
	Token token = {.start = src, .line = lines};

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

			// #define token_equals_strlit(t, s) ((t).length == sizeof(s)-1 && memcmp((t).start, (s), sizeof(s)-1) == 0)
			// token.length == sizeof(#x)-1 && memcmp(token.start, #x, sizeof(#x)-1) == 0
			#define _(x) if(token_equals_strlit(token, #x)) token.kind = KW_ ## x; else
			KEYWORDS;
			#undef _
		}

		// strings

		else if(*src == '"') {
			src ++;
			token.slen = 0;

			while(*src) {
				if(*src == '"') {
					break;
				}
				else if(*src == '\\') {
					src ++;

					if(*src == 'n' || *src == 't' || *src == '"' || *src == '\\') {
						src ++;
						token.slen ++;
					}
					else if(*src == 'x' && isxdigit(src[1]) && isxdigit(src[2])) {
						src += 3;
					}
					else {
						error_at(&token, "unrecognized or incomplete escape character %i", *src);
					}
				}
				else {
					src ++;
					token.slen ++;
				}
			}

			if(*src != '"')
				error_at(&token, "unterminated string literal");
			else
				src ++;

			token.kind = TK_STRING;
		}

		// puncts

		#define _(x, y) else if(memcmp(token.start, x, sizeof(x)-1) == 0) { token.kind = PT_ ## y; src += sizeof(x)-1; }
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
	}

	// end of file

	token.start = src;
	token.kind = TK_EOF;
	array_push(tokens, token);

	// setting first-in-line tokens and interning identifiers

	table_init(&module->idset);

	array_for(tokens, i) {
		Token *token = tokens + i;

		if(token->line->first == 0)
			token->line->first = token;

		if(token->kind == TK_IDENT)
			token->uid = table_add(&module->idset, token, 0);
	}

	// creating string literal data

	array_for(tokens, i) {
		Token *token = tokens + i;

		if(token->kind == TK_STRING) {
			uint64_t slen = token->slen;
			uint8_t *src = token->start;
			token->sval = 0;

			for(uint64_t i=1; i < token->length; i++) {
				if(src[i] == '\\' && src[i+1] == 'x' && isxdigit(src[i+2]) && isxdigit(src[i+2])) {
					string_append_ch(
						token->sval, hexchar_to_int(src[i+2]) * 16 + hexchar_to_int(src[i+3])
					);
					i+=3;
				}
				else if(token->start[i] == '\\') {
					i ++;

					if(src[i] == 'n')
						string_append_ch(token->sval, '\n');
					else if(src[i] == 't')
						string_append_ch(token->sval, '\t');
					else if(src[i] == '"')
						string_append_ch(token->sval, '"');
					else if(src[i] == '\\')
						string_append_ch(token->sval, '\\');
					else if(src[i] == 'x')
						string_append_ch(token->sval, '\\');
				}
				else if(src[i] == '"') {
					break;
				}
				else {
					string_append_ch(token->sval, src[i]);
				}
			}
		}
	}

	//table_print(&module->idset);

	/*
	Table *idset = alloc(sizeof(Table));
	array_resize(idset->items, 4);
	idset->usage = 0;

	/*
	array_for(tokens, i) {
		Token *token = tokens + i;

		if(token->line->first == 0)
			token->line->first = token;

		if(token->kind == TK_IDENT)
			token->id = idset_add(idset, token);
	}
	*/

	//print_idset(idset);

	//module->idset = idset;
	module->lines = lines;
	module->tokens = tokens;
}

#endif