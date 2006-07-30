#include "tree.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef CHILDREN_PER_NODE
#define CHILDREN_PER_NODE 3
#endif

#define DATA_PER_NODE (CHILDREN_PER_NODE-1)

#if CHILDREN_PER_NODE > 11
#define USE_BINARY_SEARCH 1
#endif


/************************* DATA STRUCTURES **********************************/

struct _TreeNode
{
	void * data[DATA_PER_NODE];
	struct _TreeNode * parent;
	struct _TreeNode * children[CHILDREN_PER_NODE];
	int dataCount;
};

struct _Tree
{
	TreeCompareDataFunction compareData;
	TreeFreeDataFunction freeData;
	TreeNode * rootNode;
};

/************************* STATIC METHODS ***********************************/

static
TreeNode *
_MakeNode()
{
	TreeNode * ret = malloc(sizeof(TreeNode));
	memset(ret, 0, sizeof(TreeNode));
	return ret;
}

static
int
_FindPositionInNode(Tree * tree, TreeNode * node, void * data, int * pos)
{
#ifdef USE_BINARY_SEARCH
	int low = 0;
	int high = node->dataCount;
	int cmp = -1;

	while (high > low)
	{
		int cur = (high + low) >> 1;
		cmp = tree->compareData(data, node->data[cur]);
		if (cmp > 0)
		{
			low = cur+1;
		}
		else if (cmp < 0)
		{
			high = cur;
		}
		else
		{
			low = cur;
			break;
		}
	}

	*pos = low;
	return (cmp == 0);
#else
	int i = 0;
	int cmp = -1;
	for (; 
	     i < node->dataCount &&
	     (cmp = tree->compareData(data, node->data[i])) > 0;
	     i++);
	*pos = i;
	return (cmp == 0);
#endif
}

static
int
_Find(TreeIterator * iter, void * data)
{
	while (1)
	{
		if (_FindPositionInNode(iter->tree,
					iter->node, 
					data, 
					&iter->which))
		{
			return 1;
		}

		if (iter->node->children[iter->which])
		{
			iter->node = iter->node->children[iter->which];
		}
		else
		{
			return 0;
		}
	}
}

static
TreeNode *
_SplitNode(TreeNode * node)
{
	assert(node->dataCount == DATA_PER_NODE);

	TreeNode * newNode = _MakeNode();

#ifdef USE_MEM_FUNC
	memcpy(&(newNode->data[0]),
	       &(node->data[DATA_PER_NODE/2]),
	       (DATA_PER_NODE-DATA_PER_NODE/2)*sizeof(void *));
	memset(&(node->data[DATA_PER_NODE/2]),
	       0,
	       (DATA_PER_NODE-DATA_PER_NODE/2)*sizeof(void *));
	memcpy(&(newNode->children[1]),
	       &(node->children[DATA_PER_NODE/2+1]),
	       (DATA_PER_NODE-DATA_PER_NODE/2)*sizeof(TreeNode *));
	memset(&(node->children[DATA_PER_NODE/2+1]),
	       0,
	       (DATA_PER_NODE-DATA_PER_NODE/2)*sizeof(TreeNode *));
#endif
	
	int i = DATA_PER_NODE/2;
	int j = 0;
	for (; i < DATA_PER_NODE; i++, j++)
	{
#ifndef USE_MEM_FUNC
		newNode->data[j] = node->data[i];
		newNode->children[j+1] = node->children[i+1];
		node->data[i] = NULL;
		node->children[i+1] = NULL;
#endif
		if (newNode->children[j+1])
		{
			newNode->children[j+1]->parent = newNode;
		}
	}
	newNode->dataCount = (DATA_PER_NODE-DATA_PER_NODE/2);
	node->dataCount -= (DATA_PER_NODE-DATA_PER_NODE/2);

	return newNode;
}

static
void
_InsertNodeAndData(Tree * tree, 
		   TreeNode * node, 
		   TreeNode * newNode,
		   void * data)
{
	assert(node->dataCount < DATA_PER_NODE);
	assert(!newNode || tree->compareData(data, newNode->data[0]) < 0);

	if (newNode)
	{
		newNode->parent = node;
	}

	int i = 0;
	_FindPositionInNode(tree, node, data, &i);

#ifdef USE_MEM_FUNC
	memmove(&(node->data[i+1]), 
		&(node->data[i]),
		(node->dataCount-i)*sizeof(void *));
	memmove(&(node->children[i+2]), 
		&(node->children[i+1]),
		(node->dataCount-i)*sizeof(TreeNode *));
#else
	int j = node->dataCount;
	for (; j > i; j--)
	{
		node->data[j] = node->data[j-1];
		node->children[j+1] = node->children[j];
	}
#endif

	assert(!node->children[i] ||
	       tree->compareData(data, node->children[i]->data[0]) > 0);

	node->data[i] = data;
	node->children[i+1] = newNode;
	node->dataCount++;
}

static
void *
_AddDataToSplitNodes(Tree * tree,
		     TreeNode * lessNode, 
		     TreeNode * moreNode,
		     TreeNode * newNode,
		     void * data)
{
	assert(newNode == NULL ||
	       tree->compareData(data, newNode->data[0]) < 0);

	void * retData;

	if (tree->compareData(data, moreNode->data[0]) < 0)
	{
		_InsertNodeAndData(tree, lessNode, newNode, data);
		lessNode->dataCount--;
		retData = lessNode->data[lessNode->dataCount];
		lessNode->data[lessNode->dataCount] = NULL;
	}
	else
	{
		if (newNode)
		{
			newNode->parent = moreNode;
		}

		int i = 0;
		_FindPositionInNode(tree, moreNode, data, &i);

		if (i == 0)
		{
			moreNode->children[0] = newNode;
			return data;
		}
		else
		{
			retData = moreNode->data[0];
		}

#ifdef USE_MEM_FUNC
		memmove(&(moreNode->data[0]), 
			&(moreNode->data[1]), 
			i*sizeof(void *));
		memmove(&(moreNode->children[0]),
			&(moreNode->children[1]),
			i*sizeof(TreeNode *));
#else
		int j = 0;
		for (; j < i; j++)
		{
			moreNode->data[j] = moreNode->data[j+1];
			moreNode->children[j] = moreNode->children[j+1];
		}
#endif

		assert(!moreNode->children[i-1] ||
		       tree->compareData(data,
			       		 moreNode->children[i]->data[0]) > 0);

		moreNode->data[i-1] = data;
		moreNode->children[i] = newNode;
	}

	return retData;
}

static
void
_InsertAt(TreeIterator * iter, void * data)
{
	TreeNode * node = iter->node;
	TreeNode * insertNode = NULL;
	
	while (node != NULL)
	{
		// see if there's any NULL data in the current node
		if (node->dataCount == DATA_PER_NODE)
		{
			// no open data slots, split this node!
			TreeNode * newNode = _SplitNode(node);

			// insert data in split nodes
			data = _AddDataToSplitNodes(iter->tree,
						    node, 
						    newNode, 
						    insertNode,
						    data);

			insertNode = newNode;

			if (node->parent == NULL)
			{
				assert(node == iter->tree->rootNode);
				iter->tree->rootNode = _MakeNode();
				node->parent = iter->tree->rootNode;
				newNode->parent = iter->tree->rootNode;
				iter->tree->rootNode->data[0] = data;
				iter->tree->rootNode->children[0] = node;
				iter->tree->rootNode->children[1] = newNode;
				iter->tree->rootNode->dataCount = 1;
				return;
			}

			node = node->parent;
		}
		else
		{
			// insert the data and newNode
			_InsertNodeAndData( iter->tree, 
					    node, 
					    insertNode,
					    data );
			node = NULL;
		}
	}

	return;
}

static void _SetTreeIteratorToRoot(TreeIterator * iter, Tree * tree)
{
	iter->tree = tree;
	iter->node = tree->rootNode;
	iter->which = 0;
}

/************************* PUBLIC METHODS ***********************************/

Tree *
MakeTree(TreeCompareDataFunction compareData, TreeFreeDataFunction freeData)
{
	Tree * ret = malloc(sizeof(Tree));
	ret->compareData = compareData;
	ret->freeData = freeData;
	ret->rootNode = _MakeNode();
	return ret;
}

void SetTreeIteratorToBegin(TreeIterator * iter, Tree * tree)
{
	_SetTreeIteratorToRoot(iter, tree);
	IncrementTreeIterator(iter);
}

int IsTreeIteratorAtEnd(const TreeIterator * iter)
{
	return (iter->node == NULL);
}

void IncrementTreeIterator(TreeIterator * iter)
{
	while(iter->node)
	{
		if (iter->node->children[iter->which])
		{
			iter->node = iter->node->children[iter->which];
			iter->which = 0;
		}
		else
		{
			iter->which++;
		}

		while (iter->node && iter->which > iter->node->dataCount)
		{
			TreeNode * childNode = iter->node;
			iter->node = childNode->parent;
			if (iter->node)
			{
				for (iter->which = 0;
				     childNode != 
				     iter->node->children[iter->which];
				     iter->which++)
				{
					assert(iter->which <= 
					       iter->node->dataCount);
				}
				iter->which++;
			}
		}

		if (iter->node &&
		    iter->which > 0 && iter->which <= iter->node->dataCount)
		{
			return;
		}
	}
}

void *
GetDataFromTreeIterator(TreeIterator * iter)
{
	assert(iter->node && 
	       iter->which > 0 && 
	       iter->which <= iter->node->dataCount);
	return iter->node->data[iter->which-1];
}

int
InsertIntoTree(Tree * tree, void * data)
{
	TreeIterator iter;
	_SetTreeIteratorToRoot(&iter, tree);

	if (_Find(&iter, data))
	{
		return 0;
	}

	_InsertAt(&iter, data);

	return 1;
}
