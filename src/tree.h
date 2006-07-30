#ifndef TREE_H
#define TREE_H

typedef struct _Tree Tree;
typedef struct _TreeNode TreeNode;
typedef struct _TreeIterator TreeIterator;

struct _TreeIterator
{
	Tree * tree;
	TreeNode * node;
	int which;
};

typedef int (*TreeCompareDataFunction)(const void * data1, const void * data2);
typedef void (*TreeFreeDataFunction)(void * data);

Tree * MakeTree(TreeCompareDataFunction compareFunc, 
		TreeFreeDataFunction freeData);

void SetTreeIteratorToBegin(TreeIterator * iter, Tree * tree);
int IsTreeIteratorAtEnd(const TreeIterator * iter);
void IncrementTreeIterator(TreeIterator * iter);

void * GetDataFromTreeIterator(TreeIterator * iter);

int InsertIntoTree(Tree * tree, void * data);

void DeleteFromTree(Tree * tree, void * data);

#endif
