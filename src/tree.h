#ifndef TREE_H
#define TREE_H

typedef struct _TreeNode {
	void * data[2];
	struct _TreeNode * children[3];
	struct _TreeNode * parent;
} TreeNode;

typedef struct _Tree {
	TreeNode headNode;
	TreeFreeDataFunc * freeFunc;
	TreeCompareDataFunc * compareFunc;
} Tree;

typedef enum _TreeIteratorType {
	PREORDER,
	INORDER,
	POSTORDER
} TreeIteratorType;

typedef struct _TreeIterator {
	Data * data;
	/* private data */
	TreeIteratorType type;
	TreeNode * currentNode;
	int pos;
} TreeIterator;

typedef int TreeCompareDataFunc(void * left, void * right);

typedef int TreeFreeDataFunc(void * data);

Tree * newTree(TreeFreeDataFunc * freeFunc, TreeCompareDataFunc * compareFunc);

void freeTree(Tree * tree);

int insertInTree(Tree * tree, void * data);

int deleteFromTree(Tree * tree, void * needle);

void * findInTree(Tree * tree, void * needle);

TreeIterator * newTreeIterator(Tree * tree, TreeIteratorType type);

/* will return the same pointer passed in on success
 * if NULL is returned, this indicates the end of tree
 */
data * nextTreeIterator(TreeIterator * iter);

void freeTreeIterator(TreeIterator * iter);

#endif /* TREE_H */
