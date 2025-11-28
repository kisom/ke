#include <stddef.h>

#include "abuf.h"
#include "editor.h"


#ifndef KE_UNDO_H
#define KE_UNDO_H


typedef enum undo_kind {
	UNDO_INSERT  = 1 << 0,
	UNDO_UNKNOWN = 1 << 1,
} undo_kind;


typedef struct undo_node {
	undo_kind		 kind;
	size_t			 row, col;
	abuf			 text;

	struct undo_node	*next;
	struct undo_node	*parent;

} undo_node;


typedef struct undo_tree {
	undo_node	*root;		/* the start of the undo sequence */
	undo_node	*current;	/* where we are currently at */
	undo_node	*pending;	/* the current undo operations being built */
} undo_tree;


undo_node	*undo_node_new(undo_kind kind);
void		 undo_node_free(undo_node *node);
void		 undo_tree_init(undo_tree *tree);
void		 undo_tree_free(undo_tree *tree);
void		 undo_begin(undo_tree *tree, undo_kind kind);
void		 undo_prepend(undo_tree *tree, abuf *buf);
void		 undo_append(undo_tree *tree, abuf *buf);
void		 undo_prependch(undo_tree *tree, char c);
void		 undo_appendch(undo_tree *tree, char c);
void		 undo_commit(undo_tree *tree);
void		 undo_apply(struct editor *editor);
void		 editor_undo(undo_tree *tree);
void		 editor_redo(undo_tree *tree);


#endif
