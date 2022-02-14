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
static char *src_end;

static Node *new_node()
{
	Node *node = malloc(sizeof(Node));
	node->type = 0;
	node->next = 0;
	node->name = 0;
	node->first_child = 0;
	node->last_child = 0;
	node->token = 0;
	return node;
}

static void clear_node(Node *node)
{
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
}

static void add_child(Node *node, Node *child)
{
	assert(node);
	assert(child);
	
	if(node->last_child) {
		node->last_child->next = child;
	}
	else {
		node->first_child = child;
	}
	
	node->last_child = child;
}

static void delete_node(Node *node)
{
	assert(node);
	clear_node(node);
	free(node);
}

static void merge_child(Node *node, Node *child)
{
	assert(node);
	assert(child);
	
	if(!child->first_child) {
		return;
	}
	
	if(node->last_child) {
		node->last_child->next = child->first_child;
	}
	else {
		node->first_child = child->first_child;
	}
	
	node->last_child = child->first_child;
}

static Node *p_TOKEN(TokenType type, char *name)
{
	if(cur->type == type) {
		Node *node = new_node();
		node->type = 1;
		node->name = name;
		node->token = cur;
		cur ++;
		return node;
	}
	
	return 0;
}

static Node *p_LITERAL(char *text, char *name)
{
	if(token_text_equals(cur, text)) {
		Node *node = new_node();
		node->type = 1;
		node->name = name;
		node->token = cur;
		cur ++;
		return node;
	}
	
	return 0;
}

#include "autoparser.c"

Node *parse_tree(Tokens *tokens)
{
	cur = tokens->first;
	src_end = tokens->last->start + tokens->last->length;
	return p_unit(tokens->first);
}
