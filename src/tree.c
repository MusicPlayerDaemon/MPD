#include "tree.h"

static inline TreeNode * newTreeNode() {
	TreeNode * ret = malloc(sizeof(TreeNode));

	ret->data[0] = NULL;
	ret->data[1] = NULL;

	ret->children[0] = NULL;
	ret->children[1] = NULL;
	ret->children[2] = NULL;

	return ret;
}

static inline void freeTreeNode(TreeNode * node) {
	free(node);
}

Tree * newTree(TreeFreeDataFunc * freeFunc, TreeCompareDataFunc * compareFunc) {
	Tree * ret = malloc(sizeof(Tree));

	ret->headNode = NULL;
	ret->freeFunc = freeFunc;
	ret->compareFunc = compareFunc;

	return ret;
}

void freeTree(Tree * tree) {
	TreeIterator * iter = newTreeIterator(tree, POSTORDER);

	TreeNode * node;

	if(data->freeFunc) {
		void * data;

		while( ( data = nextTreeIterator(iter) ) ) {

		}
	
		freeTreeIterator(iter);

		iter = newTreeIterator(tree, POSTORDER);
	}

	while( ( node = nextNodeTreeIterator(iter) ) ) {
		freeTreeNode(node);
	}

	freeTreeIterator(iter);
}
