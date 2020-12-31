#ifndef __KERNEL_LIB_RBTREE_H__
#define __KERNEL_LIB_RBTREE_H__

typedef unsigned long rbtree_key_t;

typedef struct rbtree_node_s {
	rbtree_key_t			key;
	unsigned int			color;
	struct rbtree_node_s	*parent;
	struct rbtree_node_s	*left;
	struct rbtree_node_s	*right;
} rbtree_node_t;


typedef void (*rbtree_insert_pt) (rbtree_node_t *root,
		rbtree_node_t *node, rbtree_node_t *sentinel);

typedef struct {
	rbtree_node_t 		*root;
	rbtree_node_t		*sentinel;
} rbtree_t;

#define rbtree_init(tree, s, i)			\
	rbtree_sentinel_init(s);			\
	(tree)->root = s;					\
	(tree)->sentinel = s;

void rbtree_insert(rbtree_t *tree, rbtree_node_t *node);
void rbtree_delete(rbtree_t *tree, rbtree_node_t *node);
void rbtree_insert_value(rbtree_node_t *tree, rbtree_node_t *node,
		rbtree_node_t *sentinel);
void rbtree_left_rotate(rbtree_t *tree, rbtree_node_t *node);
void rbtree_right_rotate(rbtree_t *tree, rbtree_node_t *node);
rbtree_node_t *rbtree_precedessor(rbtree_t *tree, rbtree_node_t *node);
rbtree_node_t *rbtree_successor(rbtree_t *tree, rbtree_node_t *node);
#define RBTREE_COLOR_RED		1
#define RBTREE_COLOR_BLACK		0
#define rbtree_red(node)		((node)->color = RBTREE_COLOR_RED)
#define rbtree_black(node)		((node)->color = RBTREE_COLOR_BLACK)
#define rbtree_is_red(node)		((node)->color == RBTREE_COLOR_RED)
#define rbtree_is_black(node)	(!rbtree_is_red(node))
#define rbtree_copy_color(n1, n2)	((n1)->color = (n2)->color)

#define rbtree_sentinel_init(node)	rbtree_black(node)

static inline rbtree_node_t * rbtree_min(rbtree_node_t *node, rbtree_node_t *sentinel) {
	while (node->left != sentinel) {
		node = node->left;
	}
	return node;
}

static inline void rbtree_node_init(rbtree_node_t *node, rbtree_node_t *s) {
	node->parent = s;
	node->right = s;
	node->left = s;
}

void check_rbtree(void);


#endif // __KERNEL_LIB_RBTREE_H__
