#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "syntax.h"

static Symbol *new_symbol(SymbolType type, char *name, int64_t line)
{
	Symbol *symbol = malloc(sizeof(Symbol));
	symbol->next = 0;
	symbol->type = type;
	symbol->name = name;
	symbol->line = line;
	symbol->swallow = false;
	symbol->merge = false;
	return symbol;
}

static Symbol *new_nonterm(char *name, int64_t line)
{
	return new_symbol(SY_UNRESOLVED_NONTERM, name, line);
}

static Symbol *new_token(char *name, int64_t line)
{
	return new_symbol(SY_TOKEN, name, line);
}

static Symbol *new_literal(char *name, int64_t line)
{
	return new_symbol(SY_LITERAL, name, line);
}

static Symbol *new_catch(bool after, char *msg, int64_t line)
{
	Symbol *sym = new_symbol(SY_CATCH, msg, line);
	sym->after = after;
	return sym;
}

static Alt *new_alt()
{
	Alt *alt = malloc(sizeof(Alt));
	alt->next = 0;
	alt->symbols = 0;
	return alt;
}

static Rule *new_rule(char *name)
{
	Rule *rule = malloc(sizeof(Rule));
	rule->next = 0;
	rule->name = name;
	rule->alts = 0;
	return rule;
}

static Syntax *new_syntax()
{
	Syntax *syntax = malloc(sizeof(Syntax));
	syntax->rules = 0;
	return syntax;
}

static Syntax *resolve_syntax(Syntax *syntax)
{
	for(Rule *rule = syntax->rules; rule; rule = rule->next) {
		for(Alt *alt = rule->alts; alt; alt = alt->next) {
			for(Symbol *symbol = alt->symbols; symbol; symbol = symbol->next) {
				if(symbol->type == SY_UNRESOLVED_NONTERM) {
					bool resolved = false;
					
					for(Rule *r = syntax->rules; r; r = r->next) {
						if(strcmp(r->name, symbol->name) == 0) {
							symbol->rule = r;
							symbol->type = SY_NONTERM;
							resolved = true;
							break;
						}
					}
					
					if(resolved == false) {
						fprintf(
							stderr,
							"error:%li: could not resolve non-terminal %s\n",
							symbol->line,
							symbol->name
						);
						
						exit(EXIT_FAILURE);
					}
				}
			}
		}
	}
	
	return syntax;
}

static char *skimx(char *pos, int64_t *line)
{
	while(isspace(*pos)) {
		if(*pos == '\n') {
			(*line) ++;
		}
		
		pos ++;
	}
	
	return pos;
}

void print_syntax(Syntax *syntax)
{
	for(Rule *rule = syntax->rules; rule; rule = rule->next) {
		printf("%s\n", rule->name);
		
		for(Alt *alt = rule->alts; alt; alt = alt->next) {
			if(alt == rule->alts) {
				printf("  = ");
			}
			else {
				printf("  | ");
			}
			
			for(Symbol *symbol = alt->symbols; symbol; symbol = symbol->next) {
				if(symbol->type == SY_NONTERM) {
					printf("%s ", symbol->rule->name);
				}
				else if(symbol->type == SY_LITERAL) {
					printf("\"%s\" ", symbol->name);
				}
				else {
					printf("%s ", symbol->name);
				}
			}
			
			printf("\n");
		}
		
		printf("  ;\n");
	}
}

Syntax *parse_syntax(char *src)
{
	char *pos = src;
	int64_t line = 1;
	char *start = 0;
	int64_t len = 0;
	char *word = 0;
	
	#define skim() ( pos = skimx(pos, &line) )
	#define match(s) ( skim(), strncmp((s), pos, strlen(s)) == 0 )
	#define match_lower() ( skim(), islower(*pos) )
	#define match_upper() ( skim(), isupper(*pos) )
	#define match_eof() ( skim(), *pos == 0 )
	#define eat(s) ( match(s) ? (pos += strlen(s)) : 0 )
	
	#define error(m) do { \
		fprintf(stderr, "error:%li: %s\n", line, m); \
		exit(EXIT_FAILURE); \
	} while(0)
	
	#define scan_word() do { \
		skim(); \
		start = pos; \
		while(isalnum(*pos) || *pos == '_') pos ++; \
		if(pos == start) error("expected word"); \
		len = pos - start; \
		word = malloc(len + 1); \
		word[len] = 0; \
		memcpy(word, start, len); \
	} while(0)
	
	#define scan_str() do { \
		pos ++; \
		start = pos; \
		while(*pos && *pos != '"') pos ++; \
		len = pos - start; \
		word = malloc(len + 1); \
		word[len] = 0; \
		memcpy(word, start, len); \
		if(!eat("\"")) error("expected \""); \
	} while(0)
	
	Syntax *syntax = new_syntax();
	Rule *last_rule = 0;
	
	while(true) { // each rule
		scan_word();
		Rule *rule = new_rule(word);
		Alt *last_alt = 0;
		
		if(!eat("=")) {
			error("expected =");
		}
		
		while(true) { // each alt
			Alt *alt = new_alt();
			Symbol *last_symbol = 0;
			
			while(true) { // each symbol
				Symbol *symbol = 0;
				bool swallow = false;
				bool merge = false;
				bool fold = false;
				
				if(eat("-")) {
					swallow = true;
				}
				
				if(eat("+")) {
					merge = true;
				}
				
				if(eat("~")) {
					fold = true;
				}
				
				if(eat("#")) {
					scan_word();
					
					if(strcmp(word, "catch_after") == 0) {
						if(!match("\"")) {
							error("expected string");
						}
						
						scan_str();
						symbol = new_catch(true, word, line);
						swallow = true;
					}
					else if(strcmp(word, "catch_before") == 0) {
						if(!match("\"")) {
							error("expected string");
						}
						
						scan_str();
						symbol = new_catch(false, word, line);
						swallow = false;
					}
				}
				else if(match_lower()) {
					scan_word();
					symbol = new_nonterm(word, line);
				}
				else if(match_upper()) {
					scan_word();
					symbol = new_token(word, line);
				}
				else if(eat("\"")) {
					start = pos;
					
					while(*pos && *pos != '"') {
						pos ++;
					}
					
					len = pos - start;
					word = malloc(len + 1);
					word[len] = 0;
					memcpy(word, start, len);
					symbol = new_literal(word, line);
					
					if(!eat("\"")) {
						error("expected \"");
					}
				}
				else if(eat("!")) {
					symbol = new_symbol(SY_LATCH, "!", line);
				}
				
				if(symbol) {
					symbol->swallow = swallow;
					symbol->merge = merge;
					symbol->fold = fold;
					
					if(last_symbol) {
						last_symbol->next = symbol;
					}
					else {
						alt->symbols = symbol;
					}
					
					last_symbol = symbol;
				}
				else {
					break;
				}
			}
			
			if(last_alt) {
				last_alt->next = alt;
			}
			else {
				rule->alts = alt;
			}
			
			last_alt = alt;
			
			if(!eat("|")) {
				break;
			}
		}
		
		if(!eat(";")) {
			error("expected ;");
		}
		
		if(last_rule) {
			last_rule->next = rule;
		}
		else {
			syntax->rules = rule;
		}
		
		last_rule = rule;
		
		if(match_eof()) {
			break;
		}
	}
	
	//print_syntax(syntax);
	return resolve_syntax(syntax);
}
