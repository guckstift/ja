#ifndef SYNTAX_H
#define SYNTAX_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
	SY_UNRESOLVED_NONTERM,
	SY_NONTERM,
	SY_TOKEN,
	SY_LITERAL,
	SY_LATCH,
} SymbolType;

typedef struct Symbol {
	struct Symbol *next;
	int64_t line;
	bool swallow;
	bool merge;
	bool fold;
	SymbolType type;
	union { char *name; struct Rule *rule; };
} Symbol;

typedef struct Alt {
	struct Alt *next;
	Symbol *symbols;
} Alt;

typedef struct Rule {
	struct Rule *next;
	char *name;
	Alt *alts;
} Rule;

typedef struct {
	Rule *rules;
} Syntax;

Syntax *parse_syntax(char *src);

#endif
