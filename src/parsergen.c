#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>

#define PUNCTS(_) \
	_("=", ASSIGN) \
	_(";", SEMICOLON) \
	_("|", PIPE) \

typedef enum {
	INVALID,
	#define F(x, y) y,
	PUNCTS(F)
	#undef F
} Punct;

char *pos;
int64_t line;
FILE *ofs;

void p_alts();

void error(char *msg, ...)
{
	va_list args;
	va_start(args, msg);
	fprintf(stderr, "error:%li: ", line);
	vfprintf(stderr, msg, args);
	fprintf(stderr, "\n");
	va_end(args);
	exit(EXIT_FAILURE);
}

void skim()
{
	while(isspace(*pos)) {
		if(*pos == '\n') line ++;
		pos ++;
	}
}

int is_lower(char c)
{
	return islower(c) || c == '_';
}

int is_upper(char c)
{
	return isupper(c) || c == '_';
}

char *rip_string(char *start, char *end)
{
	int64_t len = end - start;
	char *word = malloc(len + 1);
	word[len] = 0;
	memcpy(word, start, len);
	return word;
}

char *mp_nonterm()
{
	skim();
	char *start = pos;
	while(is_lower(*pos)) pos ++;
	if(pos == start) return 0;
	char *word = rip_string(start, pos);
	skim();
	return word;
}

char *p_nonterm()
{
	char *res = mp_nonterm();
	if(!res) error("expected nonterm");
	return res;
}

char *mp_token()
{
	skim();
	char *start = pos;
	while(is_upper(*pos)) pos ++;
	if(pos == start) return 0;
	char *word = rip_string(start, pos);
	skim();
	return word;
}

char *mp_literal()
{
	skim();
	if(*pos != '"') return 0;
	pos ++;
	char *start = pos;
	while(*pos && *pos != '"') pos ++;
	if(pos == start) error("expected chars after \"");
	if(*pos != '"') error("expected \" after literal");
	char *word = rip_string(start, pos);
	pos ++;
	skim();
	return word;
}

Punct mp_punct()
{
	skim();
	char *start = pos;
	while(ispunct(*pos)) pos ++;
	if(pos == start) return INVALID;
	char *word = rip_string(start, pos);
	skim();
	
	#define F(x, y) if(strcmp(word, x) == 0) return y;
	PUNCTS(F)
	#undef F
	
	pos = start;
	return INVALID;
}

Punct p_punct()
{
	Punct punct = mp_punct();
	if(punct == INVALID) error("invalid punct");
}

int eat_punct(Punct punct)
{
	int64_t startline = line;
	char *start = pos;
	Punct test = mp_punct();
	if(punct == test) return 1;
	pos = start;
	line = startline;
	return 0;
}

int mp_atom()
{
	char *nonterm = mp_nonterm();
	if(nonterm) {
		fprintf(
			ofs,
			"\t\tif(!(child = p_%s())) "
			"{ cur = start; clear_node(node); break; }\n",
			nonterm
		);
		fprintf(ofs, "\t\tadd_child(node, child);\n");
		return 1;
	}
	char *token = mp_token();
	if(token) {
		fprintf(
			ofs,
			"\t\tif(!(child = p_TOKEN(TK_%s))) "
			"{ cur = start; clear_node(node); break; }\n",
			token
		);
		fprintf(ofs, "\t\tchild->name = \"%s\";\n", token);
		fprintf(ofs, "\t\tadd_child(node, child);\n");
		return 1;
	}
	char *literal = mp_literal();
	if(literal) {
		fprintf(
			ofs,
			"\t\tif(!(child = p_LITERAL(\"%s\"))) "
			"{ cur = start; clear_node(node); break; }\n",
			literal
		);
		fprintf(ofs, "\t\tchild->name = \"\\\"%s\\\"\";\n", literal);
		fprintf(ofs, "\t\tadd_child(node, child);\n");
		return 1;
	}
	return 0;
}

void p_seq()
{
	fprintf(ofs, "\tdo {\n");
	while(*pos) {
		if(!mp_atom()) break;
	}
	fprintf(ofs, "\t\treturn node;\n");
	fprintf(ofs, "\t} while(0);\n");
}

void p_alts()
{
	while(*pos) {
		p_seq();
		if(!eat_punct(PIPE)) break;
	}
}

void p_rule()
{
	char *nonterm = p_nonterm();
	if(!eat_punct(ASSIGN)) error("expected =");
	fprintf(ofs, "\nstatic Node *p_%s() {\n", nonterm);
	//fprintf(ofs, "\tprintf(\"%s\\n\");\n", nonterm);
	fprintf(ofs, "\tToken *start = cur;\n");
	fprintf(ofs, "\tNode *node = new_node();\n");
	fprintf(ofs, "\tnode->name = \"%s\";\n", nonterm);
	fprintf(ofs, "\tNode *child = 0;\n");
	p_alts();
	if(!eat_punct(SEMICOLON)) error("expected ;");
	fprintf(ofs, "\tdelete_node(node);\n");
	fprintf(ofs, "\treturn 0;\n");
	fprintf(ofs, "}\n");
}

void gen_parser(char *grammar)
{
	pos = grammar;
	line = 1;
	
	while(*pos) {
		p_rule();
	}
}

void gen_protos(char *grammar)
{
	pos = grammar;
	line = 1;
	fprintf(ofs, "\n");
	
	while(*pos) {
		skim();
		char *nonterm = p_nonterm();
		fprintf(ofs, "static Node *p_%s();\n", nonterm);
		
		while(*pos) {
			if(*pos == '"') {
				pos ++;
				while(*pos && *pos != '"') pos ++;
				if(*pos == '"') pos ++;
			}
			else if(*pos == ';') {
				pos ++;
				break;
			}
			else {
				if(*pos == '\n') line ++;
				pos ++;
			}
		}
		
		skim();
	}
}

int main(int argc, char *argv[])
{
	if(argc < 2) error("no grammar input file");
	if(argc < 3) error("no parser output file");
	if(argc > 3) error("too many arguments");
	
	char *filename = argv[1];
	char *outfilename = argv[2];
	FILE *fs = fopen(filename, "rb");
	if(!fs) error("could not find grammar file");
	
	fseek(fs, 0, SEEK_END);
	int64_t len = ftell(fs);
	rewind(fs);
	char *grammar = malloc(len + 1);
	grammar[len] = 0;
	fread(grammar, 1, len, fs);
	fclose(fs);
	
	ofs = fopen(outfilename, "wb");
	
	fprintf(ofs,
		"#include <stdio.h>\n\n"
		"#include \"ast.h\"\n\n"
		"static Token *cur;\n\n"
		"static Node *p_TOKEN(TokenType type) {\n"
		"\tif(cur->type == type) {\n"
		"\t\tNode *node = new_node();\n"
		"\t\tnode->token = cur;\n"
		"\t\tnode->type = 1;\n"
		"\t\tcur ++;\n"
		"\t\treturn node;\n"
		"\t}\n"
		"\treturn 0;\n"
		"}\n\n"
		"static Node *p_LITERAL(char *text) {\n"
		"\tif(token_text_equals(cur, text)) {\n"
		"\t\tNode *node = new_node();\n"
		"\t\tnode->token = cur;\n"
		"\t\tnode->type = 1;\n"
		"\t\tcur ++;\n"
		"\t\treturn node;\n"
		"\t}\n"
		"\treturn 0;\n"
		"}\n"
	);
	
	gen_protos(grammar);
	gen_parser(grammar);
	
	fprintf(ofs,
		"\nNode *parse_tree(Tokens *tokens) {\n"
		"\tcur = tokens->first;\n"
		"\treturn p_unit();\n"
		"}\n"
	);
	
	fclose(ofs);
}
