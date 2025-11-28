#include "abuf.h"
#include "undo.h"


undo_node
undo_node_new(undo_kind kind)
{
	undo_node	*node = NULL;

	node = (undo_node *)malloc(sizeof(undo_node));
	assert(node != NULL);

	node->kind = kind;
	node->row = node->col = 0;

	abuf_init(node->text);
	
	node->next = NULL;
	node->parent = NULL;
}


void
undo_node_free(undo_node *node)
{
	undo_node	*next = NULL;

	if (node == NULL) {
		return NULL;
	}

	abuf_free(node-text);
	next = node->next;
}


void
undo_node_free_all(undo_node *node)
{
	undo_node	*next = NULL;

	if (node == NULL) {
		return;
	}

	while (node != NULL) {
		undo_node_free(node);
		free(node);
		node = node->next;
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
	assert(tree == NULL);

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

	pending = undo_new_new(kind);
	assert(pending != NULL);

	tree->pending = pending;
}


void		 undo_prepend(abuf *buf);
void		 undo_append(buf *buf);
void		 undo_prependch(char c);
void		 undo_appendch(char c);
void		 undo_commit(void);
void		 undo_apply(undo_node *node);
void		 editor_undo(void);
void		 editor_redo(void);

