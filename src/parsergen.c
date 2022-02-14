#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "syntax.h"

void gen_parser(Syntax *syntax)
{
	for(Rule *rule = syntax->rules; rule; rule = rule->next) {
		printf(
			"static Node *p_%s();\n"
			,rule->name
		);
	}
	
	for(Rule *rule = syntax->rules; rule; rule = rule->next) {
		printf(
			"\nstatic Node *p_%s() {\n"
			"	Token *start = cur;\n"
			"	Node *node = new_node();\n"
			"	node->name = \"%s\";\n"
			"	Node *child = 0;\n"
			,rule->name
			,rule->name
		);
		
		for(Alt *alt = rule->alts; alt; alt = alt->next) {
			bool latch = false;
			
			printf(
				"	cur = start;\n"
				"	clear_node(node);\n"
				"	do {\n"
			);
			
			for(Symbol *symbol = alt->symbols; symbol; symbol = symbol->next) {
				if(symbol->type == SY_NONTERM) {
					printf(
						"		if(!(child = p_%s())) {\n"
						,symbol->rule->name
					);
					
					if(latch) {
						printf(
							"			fatal_at(cur, \"expected %s\");\n"
							"		}\n"
							,symbol->rule->name
						);
					}
				}
				else if(symbol->type == SY_TOKEN) {
					printf(
						"		if(!(child = p_TOKEN(TK_%s, \"%s\"))) {\n"
						,symbol->name
						,symbol->name
					);
					
					if(latch) {
						printf(
							"			fatal_at(cur, \"expected %s\");\n"
							"		}\n"
							,symbol->name
						);
					}
				}
				else if(symbol->type == SY_LITERAL) {
					printf(
						"		if(!(child = p_LITERAL("
							"\"%s\", \"\\\"%s\\\"\"))) {\n"
						,symbol->name
						,symbol->name
					);
					
					if(latch) {
						printf(
							"			fatal_at("
								"cur, \"expected \\\"%s\\\"\");\n"
							"		}\n"
							,symbol->name
						);
					}
				}
				else if(symbol->type == SY_LATCH) {
					latch = true;
					continue;
				}
				
				if(!latch) {
					printf(
						"			break;\n"
						"		}\n"
					);
				}
				
				if(!symbol->swallow) {
					if(symbol->merge) {
						printf(
							"		merge_child(node, child);\n"
							"		free(child);\n"
						);
					}
					else if(symbol->fold) {
						printf(
							"		if(child->first_child && "
								"child->first_child->next == 0) {\n"
							"			merge_child(node, child);\n"
							"			free(child);\n"
							"		}\n"
							"		else if(child->first_child) {\n"
							"			add_child(node, child);\n"
							"		}\n"
						);
					}
					else {
						printf(
							"		add_child(node, child);\n"
						);
					}
				}
			}
			
			printf(
				"		return node;\n"
				"	} while(0);\n"
			);
		}
		
		printf(
			"	delete_node(node);\n"
			"	return 0;\n"
			"}\n"
		);
	}
}

int main(int argc, char *argv[])
{
	fseek(stdin, 0, SEEK_END);
	int64_t len = ftell(stdin);
	rewind(stdin);
	char *src = malloc(len + 1);
	src[len] = 0;
	fread(src, 1, len, stdin);
	Syntax *syntax = parse_syntax(src);
	gen_parser(syntax);
	return 0;
}
