#ifndef list_H
#define list_H

#ifndef IMPLEMENT_FLAG
#define IMPLEMENT_FLAG
#define list_C
#endif

#include <stdint.h>

/*
#define Node(T) \
	struct { \
		void *next; \
		T *item; \
	}

#define List(T) \
	struct { \
		Node(T) *first; \
		Node(T) *last; \
		int64_t length; \
	}
*/

//#define list_for(l, n) \
//	for(typeof(*l->first->item) *n = (l)->first; n; n = n->next)

typedef struct Node {
	void *item;
	struct Node *next;
} Node;

typedef struct {
	Node *first;
	Node *last;
	int64_t length;
} List;

void list_push(void *list, void *item);

#endif
#ifdef list_C

#include "arena.c"

/*
void list_push(void *list, void *item)
{
	List(void) *vlist = list;
	Node(void) *node = alloc(sizeof(Node(void)));
	node->item = item;
	vlist->length ++;

	if(vlist->first)
		vlist->last = (void*)(vlist->last->next = (void*)node);
	else
		vlist->last = (void*)(vlist->first = (void*)node);
}
*/

typedef struct {
	char *str;
	int64_t length;
} Foo;

static void test_lists()
{
	Foo foo1 = {"Hello", 5};
	Foo foo2 = {"World!", 6};

	/*
	List(Foo) foos = {};
	list_push(&foos, &foo1);
	list_push(&foos, &foo2);
	*/

	//list_for(&foos, n);
}

#endif