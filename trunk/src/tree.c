/* the Music Player Daemon (MPD)
 * Copyright (C) 2006-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "tree.h"
#include "utils.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifndef CHILDREN_PER_NODE
#define CHILDREN_PER_NODE 25
#endif

#define DATA_PER_NODE (CHILDREN_PER_NODE-1)

#if CHILDREN_PER_NODE > 7
#define USE_BINARY_SEARCH 1
#endif


/************************* DATA STRUCTURES **********************************/

struct _TreeNode
{
	TreeKeyData keyData[DATA_PER_NODE];
	struct _TreeNode * parent;
	short parentPos;
	struct _TreeNode * children[CHILDREN_PER_NODE];
	short count;
};

struct _Tree
{
	TreeCompareKeyFunction compareKey;
	TreeFreeFunction freeKey;
	TreeFreeFunction freeData;
	TreeNode * rootNode;
	int size;
};

/************************* STATIC METHODS ***********************************/

static
TreeNode *
_MakeNode(void)
{
	TreeNode * ret = xmalloc(sizeof(TreeNode));
	memset(ret, 0, sizeof(TreeNode));
	return ret;
}

static
void
_ClearKeyData(TreeKeyData * keyData)
{
	memset(keyData, 0, sizeof(TreeKeyData));
}

static
int
_FindPosition(Tree * tree, TreeNode * node, void * key, int * pos)
{
#ifdef USE_BINARY_SEARCH
	int low = 0;
	int high = node->count;
	int cmp = -1;

	while (high > low)
	{
		int cur = (high + low) >> 1;
		cmp = tree->compareKey(key, node->keyData[cur].key);
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
	     i < node->count &&
	     (cmp = tree->compareKey(key, node->keyData[i].key)) > 0;
	     i++);
	*pos = i;
	return (cmp == 0);
#endif
}

static
int
_Find(TreeIterator * iter, void * key)
{
	while (1)
	{
		if (_FindPosition(iter->tree, iter->node, key, &iter->which))
		{
			iter->which++;
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

static void _SetIteratorToRoot(Tree * tree, TreeIterator * iter)
{
	iter->tree = tree;
	iter->node = tree->rootNode;
	iter->which = 0;
}

static
TreeNode *
_SplitNode(TreeNode * node)
{
	TreeNode *newNode = _MakeNode();
	int i = DATA_PER_NODE/2;
	int j = 0;

	assert(node->count == DATA_PER_NODE);

	for (; i < DATA_PER_NODE; i++, j++)
	{
		newNode->keyData[j] = node->keyData[i];
		newNode->children[j+1] = node->children[i+1];
		if (newNode->children[j+1])
		{
			newNode->children[j+1]->parent = newNode;
			newNode->children[j+1]->parentPos = j+1;
		}
		_ClearKeyData(&(node->keyData[i]));
		node->children[i+1] = NULL;
	}
	newNode->count = (DATA_PER_NODE-DATA_PER_NODE/2);
	node->count -= (DATA_PER_NODE-DATA_PER_NODE/2);

	return newNode;
}

static
void
_InsertNodeAndData(Tree * tree, 
		   TreeNode * node,
		   int pos,
		   TreeNode * newNode,
		   TreeKeyData keyData)
{
	int j = node->count;

	assert(node->count < DATA_PER_NODE);

	for (; j > pos; j--)
	{
		node->keyData[j] = node->keyData[j-1];
		node->children[j+1] = node->children[j];
		if (node->children[j+1])
		{
			node->children[j+1]->parentPos = j+1;
		}
	}

	node->keyData[pos] = keyData;
	node->count++;

	node->children[pos+1] = newNode;
	if (newNode)
	{
		newNode->parent = node;
		newNode->parentPos = pos+1;
	}
}

static
TreeKeyData
_AddDataToSplitNodes(Tree * tree,
		     TreeNode * lessNode, 
		     TreeNode * moreNode,
		     int pos,
		     TreeNode * newNode,
		     TreeKeyData keyData)
{
	TreeKeyData retKeyData;

	assert(moreNode->children[0] == NULL);

	if (pos <= lessNode->count)
	{
		_InsertNodeAndData(tree, lessNode, pos, newNode, keyData);
		lessNode->count--;
		retKeyData = lessNode->keyData[lessNode->count];
		_ClearKeyData(&(lessNode->keyData[lessNode->count]));
		moreNode->children[0] = 
			lessNode->children[lessNode->count+1];
		if (moreNode->children[0])
		{
			moreNode->children[0]->parent = moreNode;
			moreNode->children[0]->parentPos = 0;
		}
		lessNode->children[lessNode->count+1] = NULL;
	}
	else
	{
		int j;

		pos -= lessNode->count;
		retKeyData = moreNode->keyData[0];
		assert(!moreNode->children[0]);

		for (j = 0; j < pos; j++)
		{
			moreNode->keyData[j] = moreNode->keyData[j+1];
			moreNode->children[j] = moreNode->children[j+1];
			if (moreNode->children[j])
			{
				moreNode->children[j]->parentPos = j;
			}
		}

		moreNode->keyData[pos-1] = keyData;
		moreNode->children[pos] = newNode;
		if (newNode)
		{
			newNode->parent = moreNode;
			newNode->parentPos = pos;
		}
	}

	return retKeyData;
}

static
void
_InsertAt(TreeIterator * iter, TreeKeyData keyData)
{
	TreeNode * node = iter->node;
	TreeNode * insertNode = NULL;
	int pos = iter->which;
	
	while (node != NULL)
	{
		/* see if there's any NULL data in the current node */
		if (node->count == DATA_PER_NODE)
		{
			/* no open data slots, split this node! */
			TreeNode * newNode = _SplitNode(node);

			/* insert data in split nodes */
			keyData = _AddDataToSplitNodes(iter->tree,
						       node, 
						       newNode,
						       pos,
						       insertNode,
						       keyData);

			if (node->parent == NULL)
			{
				assert(node == iter->tree->rootNode);
				iter->tree->rootNode = _MakeNode();
				iter->tree->rootNode->children[0] = node;
				node->parent = iter->tree->rootNode;
				node->parentPos = 0;
				iter->tree->rootNode->children[1] = newNode;
				newNode->parent = iter->tree->rootNode;
				newNode->parentPos = 1;
				iter->tree->rootNode->keyData[0] = keyData;
				iter->tree->rootNode->count = 1;
				return;
			}

			pos = node->parentPos;
			node = node->parent;
			insertNode = newNode;
		}
		else
		{
			/* insert the data and newNode */
			_InsertNodeAndData(iter->tree, 
					   node,
					   pos,
					   insertNode,
					   keyData);
			return;
		}
	}
}

static
void
_MergeNodes(TreeNode * lessNode, TreeNode * moreNode)
{
	int i = 0;
	int j = lessNode->count;

	assert((lessNode->count + moreNode->count) <= DATA_PER_NODE);
	assert(lessNode->children[j] == NULL);

	for(; i < moreNode->count; i++,j++)
	{
		assert(!lessNode->children[j]);
		lessNode->keyData[j] = moreNode->keyData[i];
		lessNode->children[j] = moreNode->children[i];
		if (lessNode->children[j])
		{
			lessNode->children[j]->parent = lessNode;
			lessNode->children[j]->parentPos = j;
		}
	}
	lessNode->children[j] = moreNode->children[i];
	if (lessNode->children[j])
	{
		lessNode->children[j]->parent = lessNode;
		lessNode->children[j]->parentPos = j;
	}
	lessNode->count += i;

	free(moreNode);
}

static void _DeleteAt(TreeIterator * iter)
{
	TreeNode * node = iter->node;
	int pos = iter->which - 1;
	TreeKeyData * keyData = &(node->keyData[pos]);
	TreeKeyData keyDataToFree = *keyData;
	int i;

	{
		/* find the least greater than data to fill the whole! */
		if (node->children[pos+1])
		{
			TreeNode * child = node->children[++pos];
			while (child->children[0])
			{
				pos = 0;
				child = child->children[0];
			}

			*keyData = child->keyData[0];
			keyData = &(child->keyData[0]);
			node = child;
		}
		/* or the greatest lesser than data to fill the whole! */
		else if (node->children[pos])
		{
			TreeNode * child = node->children[pos];
			while (child->children[child->count])
			{
				pos = child->count;
				child = child->children[child->count];
			}

			*keyData = child->keyData[child->count-1];
			keyData = &(child->keyData[child->count-1]);
			node = child;
		}
		else
		{
			pos = node->parentPos;
		}
	}

	/* move data nodes over, we're at a leaf node, so we can ignore
	   children */
	i = keyData - node->keyData;
	for (; i < node->count-1; i++)
	{
		node->keyData[i] = node->keyData[i+1];
	}
	_ClearKeyData(&(node->keyData[--node->count]));

	/* merge the nodes from the bottom up which have too few data */
	while (node->count < (DATA_PER_NODE/2))
	{
		/* if we're not the root */
		if (node->parent)
		{
			TreeNode ** child = &(node->parent->children[pos]);
			assert(node->parent->children[pos] == node);

			/* check siblings for extra data */
			if (pos < node->parent->count &&
			    (*(child+1))->count > (DATA_PER_NODE/2))
			{
				child++;
				node->keyData[node->count++] = 
					node->parent->keyData[pos];
				node->children[node->count] =
					(*child)->children[0];
				if (node->children[node->count])
				{
					node->children[node->count]->
						parent = node;
					node->children[node->count]->
						parentPos = node->count;
				}
				node->parent->keyData[pos] =
					(*child)->keyData[0];
				i = 0;
				for(; i < (*child)->count-1; i++)
				{
					(*child)->keyData[i] = 
						(*child)->keyData[i+1];
					(*child)->children[i] =
						(*child)->children[i+1];
					if ((*child)->children[i])
					{
						(*child)->children[i]->
							parentPos = i;
					}
				}
				(*child)->children[i] = (*child)->children[i+1];
				if ((*child)->children[i])
				{
					(*child)->children[i]->parentPos = i;
				}
				(*child)->children[i+1] =NULL;
				_ClearKeyData(&((*child)->keyData[i]));
				(*child)->count--;
			}
			else if (pos > 0 &&
				 (*(child-1))->count>(DATA_PER_NODE/2))
			{
				child--;
				i = node->count++;
				for(; i > 0; i--)
				{
					node->keyData[i] = node->keyData[i-1];
					node->children[i+1] = node->children[i];
					if (node->children[i+1])
					{
						node->children[i+1]->parentPos =
							i+1;
					}
				}
				node->children[1] = node->children[0];
				if (node->children[1])
				{
					node->children[1]->parentPos = 1;
				}
				node->keyData[0] = node->parent->keyData[pos-1];
				node->children[0] = 
					(*child)->children[(*child)->count];
				if (node->children[0])
				{
					node->children[0]->parent = node;
					node->children[0]->parentPos = 0;
				}
				node->parent->keyData[pos-1] = 
					(*child)->keyData[(*child)->count-1];
				(*child)->children[(*child)->count--] =
					NULL;
				_ClearKeyData(
					&((*child)->keyData[(*child)->count]));
			}
			/* merge with one of our siblings */
			else
			{
				if (pos < node->parent->count)
				{
					child++;
					assert(*child);

					node->keyData[node->count++] =
						node->parent->keyData[pos];

					_MergeNodes(node, *child);
				}
				else
				{
					assert(pos > 0);
					child--;
					assert(*child);
					pos--;

					(*child)->keyData[(*child)->count++] = 
						node->parent->keyData[pos];

					_MergeNodes(*child, node);
					node = *child;
				}

				i = pos;
				for(; i < node->parent->count-1; i++)
				{
					node->parent->keyData[i] =
						node->parent->keyData[i+1];
					node->parent->children[i+1] =
						node->parent->children[i+2];
					if (node->parent->children[i+1])
					{
						node->parent->children[i+1]->
							parentPos = i+1;
					}
				}
				_ClearKeyData(&(node->parent->keyData[i]));
				node->parent->children[i+1] = NULL;
				node->parent->count--;

				node = node->parent;
				pos = node->parentPos;
			}
		}
		/* this is a root node */
		else 
		{
			if (node->count == 0)
			{
				if (node->children[0])
				{
					node->children[0]->parent = NULL;
					node->children[0]->parentPos = 0;
				}

				iter->tree->rootNode = node->children[0];

				free(node);
			}

			break;
		}
	}

	if (iter->tree->freeKey)
	{
		iter->tree->freeData(keyDataToFree.key);
	}
	if (iter->tree->freeData)
	{
		iter->tree->freeData(keyDataToFree.data);
	}
}

/************************* PUBLIC METHODS ***********************************/

Tree *
MakeTree(TreeCompareKeyFunction compareKey,
	 TreeFreeFunction freeKey,
	 TreeFreeFunction freeData)
{
	Tree * ret = xmalloc(sizeof(Tree));
	ret->compareKey = compareKey;
	ret->freeKey = freeKey;
	ret->freeData = freeData;
	ret->rootNode = _MakeNode();
	ret->size = 0;
	return ret;
}

void
FreeTree(Tree * tree)
{
	assert(tree->rootNode == NULL);
	free(tree);
}

int
GetTreeSize(Tree * tree)
{
	return tree->size;
}

void SetTreeIteratorToBegin(Tree * tree, TreeIterator * iter)
{
	_SetIteratorToRoot(tree, iter);
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

		while (iter->node && iter->which > iter->node->count)
		{
			iter->which = iter->node->parentPos + 1;
			iter->node = iter->node->parent;
		}

		if (iter->node &&
		    iter->which > 0 && iter->which <= iter->node->count)
		{
			return;
		}
	}
}

TreeKeyData
GetTreeKeyData(TreeIterator * iter)
{
	assert(iter->node && 
	       iter->which > 0 && 
	       iter->which <= iter->node->count);
	return iter->node->keyData[iter->which-1];
}

int
InsertInTree(Tree * tree, void * key, void * data)
{
	TreeKeyData keyData;
	TreeIterator iter;

	_SetIteratorToRoot(tree, &iter);

	if (_Find(&iter, key))
	{
		return 0;
	}

	keyData.key = key;
	keyData.data = data;
	_InsertAt(&iter, keyData);
	tree->size++;

	return 1;
}

int
RemoveFromTreeByKey(Tree * tree, void * key)
{
	TreeIterator iter;
	_SetIteratorToRoot(tree, &iter);

	if (_Find(&iter, key))
	{
		_DeleteAt(&iter);
		tree->size--;
		return 1;
	}

	return 0;
}

void
RemoveFromTreeByIterator(Tree * tree, TreeIterator * iter)
{
	_DeleteAt(iter);
	tree->size--;
}

int
FindInTree(Tree * tree, void * key, TreeIterator * iter)
{
	TreeIterator i;
	
	if (iter == NULL)
	{
		iter = &i;
	}

	_SetIteratorToRoot(tree, iter);
	if (_Find(iter, key))
	{
		return 1;
	}

	return 0;
}
