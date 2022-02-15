#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "syntax.h"

void gen_parser(Syntax *syntax)
{
	for(Rule *rule = syntax->rules; rule; rule = rule->next) {
		printf(
			"static Node p_%s();\n"
			,rule->name
		);
	}
	
	for(Rule *rule = syntax->rules; rule; rule = rule->next) {
		bool catch = false;
		char *msg = "";
		bool after = false;
		
		printf(
			"\nstatic Node p_%s() {\n"
			"	Token *start = cur;\n"
			"	Node node = new_nonterm_node(\"%s\");\n"
			"	Node child = {0};\n"
			,rule->name
			,rule->name
		);
		
		for(Alt *alt = rule->alts; alt; alt = alt->next) {
			bool latch = false;
			
			printf(
				"	cur = start;\n"
				"	clear_node(&node);\n"
				"	do {\n"
			);
			
			for(Symbol *symbol = alt->symbols; symbol; symbol = symbol->next) {
				if(symbol->type == SY_NONTERM) {
					printf(
						"		if((child = p_%s()).type == ND_INVALID) {\n"
						,symbol->rule->name
					);
					
					if(catch) {
						printf(
							"			fatal_%s, \"%s\");\n"
							"		}\n"
							,after ? "after(last" : "at(cur"
							,msg
						);
					}
					else if(latch) {
						printf(
							"			fatal_at(cur, \"expected %s\");\n"
							"		}\n"
							,symbol->rule->name
						);
					}
				}
				else if(symbol->type == SY_TOKEN) {
					printf(
						"		if((child = p_TOKEN(TK_%s, \"%s\")).type == "
							"ND_INVALID) {\n"
						,symbol->name
						,symbol->name
					);
					
					if(catch) {
						printf(
							"			fatal_%s, \"%s\");\n"
							"		}\n"
							,after ? "after(last" : "at(cur"
							,msg
						);
					}
					else if(latch) {
						printf(
							"			fatal_at(cur, \"expected %s\");\n"
							"		}\n"
							,symbol->name
						);
					}
				}
				else if(symbol->type == SY_LITERAL) {
					printf(
						"		if((child = p_LITERAL("
							"\"%s\", \"\\\"%s\\\"\")).type == ND_INVALID) {\n"
						,symbol->name
						,symbol->name
					);
					
					if(catch) {
						printf(
							"			fatal_%s, \"%s\");\n"
							"		}\n"
							,after ? "after(last" : "at(cur"
							,msg
						);
					}
					else if(latch) {
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
				else if(symbol->type == SY_CATCH) {
					catch = true;
					msg = symbol->msg;
					after = symbol->after;
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
							"		merge_child(&node, child);\n"
							//"		free(child);\n"
						);
					}
					else if(symbol->fold) {
						printf(
							"		if(child.child_count == 1) {\n"
							"			merge_child(&node, child);\n"
							//"			free(child);\n"
							"		}\n"
							"		else if(child.child_count > 1) {\n"
							"			add_child(&node, child);\n"
							"		}\n"
						);
					}
					else {
						printf(
							"		add_child(&node, child);\n"
						);
					}
				}
				
				catch = false;
			}
			
			printf(
				"		return node;\n"
				"	} while(0);\n"
			);
		}
		
		printf(
			"	delete_node(&node);\n"
			"	return new_invalid_node();\n"
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
	
	if(argc >= 2 && strcmp(argv[1] , "-p") == 0) {
		print_syntax(syntax);
		return 0;
	}
	
	gen_parser(syntax);
	return 0;
}
