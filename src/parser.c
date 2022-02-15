#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include "parser.h"
#include "utils.h"
#include "print.h"

#define error(line, linep, start, ...) \
	print_error(line, linep, src_end, start, __VA_ARGS__)

#define error_at(token, ...) \
	error((token)->line, (token)->linep, (token)->start, __VA_ARGS__)

#define error_after(token, ...) \
	error( \
		(token)->line, (token)->linep, (token)->start + (token)->length, \
		__VA_ARGS__ \
	)

#define fatal(line, linep, start, ...) do { \
	error(line, linep, start, __VA_ARGS__); \
	exit(EXIT_FAILURE); \
} while(0)

#define fatal_at(token, ...) \
	fatal((token)->line, (token)->linep, (token)->start, __VA_ARGS__)

#define fatal_after(token, ...) \
	fatal( \
		(token)->line, (token)->linep, (token)->start + (token)->length, \
		__VA_ARGS__ \
	)

static void delete_node(Node *node);

static Token *cur;
static Token *last;
static char *src_end;

static Node new_invalid_node()
{
	return (Node){
		.type = ND_INVALID,
		.name = 0,
		.child_count = 0,
		.children = 0,
	};
}

static Node new_nonterm_node(char *name)
{
	return (Node){
		.type = ND_NONTERM,
		.name = name,
		.child_count = 0,
		.children = 0,
	};
}

static Node new_token_node(char *name, Token *token)
{
	return (Node){
		.type = ND_TOKEN,
		.name = name,
		.child_count = 0,
		.token = token,
	};
}

static void clear_node(Node *node)
{
	/*
	assert(node);
	
	if(node->type == 0) {
		for(Node *child = node->first_child; child;) {
			Node *next = child->next;
			delete_node(child);
			child = next;
		}
		
		node->first_child = 0;
		node->last_child = 0;
	}
	*/
}

static void add_child(Node *node, Node child)
{
	assert(node);
	
	node->child_count ++;
	node->children = realloc(node->children, sizeof(Node) * node->child_count);
	node->children[node->child_count - 1] = child;
}

static void delete_node(Node *node)
{
	/*
	assert(node);
	clear_node(node);
	free(node);
	*/
}

static void merge_child(Node *node, Node child)
{
	assert(node);
	
	for(int64_t i=0; i < child.child_count; i++) {
		add_child(node, child.children[i]);
	}
}

static Node p_TOKEN(TokenType type, char *name)
{
	if(cur->type == type) {
		return new_token_node(name, last = cur ++);
	}
	
	return new_invalid_node();
}

static Node p_LITERAL(char *text, char *name)
{
	if(token_text_equals(cur, text)) {
		return new_token_node(name, last = cur ++);
	}
	
	return new_invalid_node();
}

#include "autoparser.c"

Node parse_tree(Tokens *tokens)
{
	cur = tokens->first;
	last = 0;
	src_end = tokens->last->start + tokens->last->length;
	return p_unit(tokens->first);
}
