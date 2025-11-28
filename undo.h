#ifndef KE_UNDO
#define KE_UNDO


typedef enum undo_kind {
	UNDO_INSERT  = 1 << 0,
	UNDO_UNKNOWN = 1 << 1,
} undo_kind;


typedef struct undo_node {
	undo_kind		 op;
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
void		 undo_prepend(abuf *buf);
void		 undo_append(buf *buf);
void		 undo_prependch(char c);
void		 undo_appendch(char c);
void		 undo_commit(undo_tree *tree);
void		 undo_apply(undo_node *node);
void		 editor_undo(void);
void		 editor_redo(void);


#endif
