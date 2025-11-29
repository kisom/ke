#include <assert.h>
#include <stdlib.h>
#include "abuf.h"
#include "buffer.h"
#include "undo.h"


undo_node *
undo_node_new(undo_kind kind)
{
	undo_node	*node = NULL;

	node = (undo_node *)malloc(sizeof(undo_node));
	assert(node != NULL);

	node->kind = kind;
	node->row = node->col = 0;

	ab_init(&node->text);
	
	node->next = NULL;
	node->next = NULL;

	return node;
}


void
undo_node_free(undo_node *node)
{
	if (node == NULL) {
		return;
	}

	ab_free(&node->text);
}


void
undo_node_free_all(undo_node *node)
{
	undo_node	*next = NULL;

	if (node == NULL) {
		return;
	}

	while (node != NULL) {
		next = node->next;
		undo_node_free(node);
		free(node);
		node = next;
	}
}


void
undo_tree_init(undo_tree *tree)
{
	assert(tree != NULL);

	tree->root    = NULL;
	tree->current = NULL;
	tree->pending = NULL;
}


void
undo_tree_free(undo_tree *tree)
{
	assert(tree != NULL);

	undo_node_free(tree->pending);
	undo_node_free_all(tree->root);
	undo_tree_init(tree);
}


void
undo_begin(undo_tree *tree, undo_kind kind)
{
	undo_node	*pending = NULL;
	
	if (tree->pending != NULL) {
		if (tree->pending->kind == kind) {
			/* don't initiate a new undo sequence if it's the same kind */
			return;
		}
		undo_commit(tree);
	}

	pending = undo_node_new(kind);
	assert(pending != NULL);

	tree->pending = pending;
}


void
undo_prepend(undo_tree *tree, abuf *buf)
{
	assert(tree != NULL);
	assert(tree->pending != NULL);

	ab_prepend_ab(&tree->pending->text, buf);
}


void
undo_append(undo_tree *tree, abuf *buf)
{
	assert(tree != NULL);
	assert(tree->pending != NULL);

	ab_append_ab(&tree->pending->text, buf);
}


void
undo_prependch(undo_tree *tree, char c)
{
	assert(tree != NULL);
	assert(tree->pending != NULL);

	ab_prependch(&tree->pending->text, c);
}


void
undo_appendch(undo_tree *tree, char c)
{
	assert(tree != NULL);
	assert(tree->pending != NULL);

	ab_appendch(&tree->pending->text, c);
}


void
undo_commit(undo_tree *tree)
{
	assert(tree != NULL);

	if (tree->pending == NULL) {
		return;
	}

	if (tree->root == NULL) {
		assert(tree->current == NULL);

		tree->root    = tree->pending;
		tree->current = tree->pending;
		tree->pending = NULL;

		return;
	}

	assert(tree->current != NULL);
	if (tree->current->next != NULL) {
		undo_node_free_all(tree->current->next);
	}

	tree->pending->prev = tree->current;
	tree->current->next = tree->pending;
	tree->current       = tree->pending;
	tree->pending       = NULL;
}


int
undo_apply(struct buffer *buf, int direction)
{
	(void)buf;
	(void)direction;

	return 0;
}


int
editor_undo(struct buffer *buf)
{
	if (buf == NULL) {
		return 0;
	}

	undo_commit(&buf->tree);
	return undo_apply(buf, UNDO_DIR_UNDO);
	
}


int
editor_redo(struct buffer *buf)
{
	if (buf == NULL) {
		return 0;
	}

	undo_commit(&buf->tree);
	return undo_apply(buf, UNDO_DIR_REDO);
}

