#ifndef PARSER_H
#define PARSER_H

#include "lex.h"

typedef enum {
	ND_INVALID,
	ND_NONTERM,
	ND_TOKEN,
} NodeType;

typedef struct Node {
	NodeType type;
	char *name;
	int64_t child_count;
	union {
		struct Node *children;
		Token *token;
	};
} Node;

Node parse_tree(Tokens *tokens);

#endif
