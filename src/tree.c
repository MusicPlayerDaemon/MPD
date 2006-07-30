#include "tree.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
_Find(TreeIterator * iter, void * data)
{
	while (1)
	{
		for (; iter->which < iter->node->dataCount; iter->which++)
		{
			int comp = iter->tree->
				compareData(data,iter->node->data[iter->which]);

			if (comp == 0)
			{
				return 1;
			}
			else if (comp < 0)
			{
				break;
			}
		}

		assert(iter->which < CHILDREN_PER_NODE);

		if (iter->node->children[iter->which])
		{
			iter->node = iter->node->children[iter->which];
			iter->which = 0;
		}
		else
		{
			return 0;
		}
	}
}

Tree *
MakeTree(TreeCompareDataFunction compareData, TreeFreeDataFunction freeData)
{
	Tree * ret = malloc(sizeof(Tree));
	ret->compareData = compareData;
	ret->freeData = freeData;
	ret->rootNode = _MakeNode();
	return ret;
}

static
TreeNode *
_SplitNode(TreeNode * node)
{
	assert(node->dataCount == DATA_PER_NODE);

	TreeNode * newNode = _MakeNode();

	unsigned i = DATA_PER_NODE/2;
	unsigned j = 0;
	for (; i < DATA_PER_NODE; i++, j++)
	{
		newNode->data[j] = node->data[i];
		newNode->children[j+1] = node->children[i+1];
		if (newNode->children[j+1])
		{
			newNode->children[j+1]->parent = newNode;
		}
		node->data[i] = NULL;
		node->children[i+1] = NULL;
	}
	newNode->dataCount = j;
	node->dataCount -= j;

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
	for (; 
	     i < node->dataCount &&
	     tree->compareData(data, node->data[i]) > 0;
	     i++);

	int j = node->dataCount;
	for (; j > i; j--)
	{
		node->data[j] = node->data[j-1];
		node->children[j+1] = node->children[j];
	}

	assert(!node->children[j] ||
	       tree->compareData(data, node->children[j]->data[0]) > 0);

	node->data[j] = data;
	node->children[j+1] = newNode;
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
		for (; 
		     i < moreNode->dataCount &&
		     tree->compareData(data, moreNode->data[i]) > 0;
		     i++);

		if (i == 0)
		{
			moreNode->children[0] = newNode;
			return data;
		}
		else
		{
			retData = moreNode->data[0];
		}

		int j = 0;
		for (; j < i; j++)
		{
			moreNode->data[j] = moreNode->data[j+1];
			moreNode->children[j] = moreNode->children[j+1];
		}

		assert(!moreNode->children[j-1] ||
		       tree->compareData(data,
			       		 moreNode->children[j]->data[0]) > 0);

		moreNode->data[j-1] = data;
		moreNode->children[j] = newNode;
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

void SetIteratorToBegin(TreeIterator * iter, Tree * tree)
{
	iter->tree = tree;
	iter->node = tree->rootNode;
	iter->which = 0;
}

int
InsertIntoTree(Tree * tree, void * data)
{
	TreeIterator iter;
	SetIteratorToBegin(&iter, tree);

	if (_Find(&iter, data))
	{
		return 0;
	}

	_InsertAt(&iter, data);

	return 1;
}
