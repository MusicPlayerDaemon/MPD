/* the Music Player Daemon (MPD)
 * (c)2006 by Warren Dukes (warren.dukes@gmail.com)
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifndef CHILDREN_PER_NODE
#define CHILDREN_PER_NODE 3
#endif

#define DATA_PER_NODE (CHILDREN_PER_NODE-1)

#if CHILDREN_PER_NODE > 7
#define USE_BINARY_SEARCH 1
#endif


/************************* DATA STRUCTURES **********************************/

struct _TreeNode
{
	void * data[DATA_PER_NODE];
	struct _TreeNode * parent;
	short parentPos;
	struct _TreeNode * children[CHILDREN_PER_NODE];
	short dataCount;
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
_FindPosition(Tree * tree, TreeNode * node, void * data, int * pos)
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
		if (_FindPosition(iter->tree, iter->node, data, &iter->which))
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

static void _SetTreeIteratorToRoot(TreeIterator * iter, Tree * tree)
{
	iter->tree = tree;
	iter->node = tree->rootNode;
	iter->which = 0;
}

static
TreeNode *
_SplitNode(TreeNode * node)
{
	assert(node->dataCount == DATA_PER_NODE);

	TreeNode * newNode = _MakeNode();

	int i = DATA_PER_NODE/2;
	int j = 0;
	for (; i < DATA_PER_NODE; i++, j++)
	{
		newNode->data[j] = node->data[i];
		newNode->children[j+1] = node->children[i+1];
		if (newNode->children[j+1])
		{
			newNode->children[j+1]->parent = newNode;
			newNode->children[j+1]->parentPos = j+1;
		}
		node->data[i] = NULL;
		node->children[i+1] = NULL;
	}
	newNode->dataCount = (DATA_PER_NODE-DATA_PER_NODE/2);
	node->dataCount -= (DATA_PER_NODE-DATA_PER_NODE/2);

	return newNode;
}

static
void
_InsertNodeAndData(Tree * tree, 
		   TreeNode * node,
		   int pos,
		   TreeNode * newNode,
		   void * data)
{
	assert(node->dataCount < DATA_PER_NODE);
#ifdef TREE_DEBUG
	assert(!newNode || tree->compareData(data, newNode->data[0]) < 0);
#endif

	int j = node->dataCount;
	for (; j > pos; j--)
	{
		node->data[j] = node->data[j-1];
		node->children[j+1] = node->children[j];
		if (node->children[j+1])
		{
			node->children[j+1]->parentPos = j+1;
		}
	}

#ifdef TREE_DEBUG
	assert(!node->children[pos] ||
	       tree->compareData(data, node->children[pos]->data[0]) > 0);
#endif

	node->data[pos] = data;
	node->dataCount++;

	node->children[pos+1] = newNode;
	if (newNode)
	{
		newNode->parent = node;
		newNode->parentPos = pos+1;
	}
}

static
void *
_AddDataToSplitNodes(Tree * tree,
		     TreeNode * lessNode, 
		     TreeNode * moreNode,
		     int pos,
		     TreeNode * newNode,
		     void * data)
{
#ifdef TREE_DEBUG
	assert(newNode == NULL ||
	       tree->compareData(data, newNode->data[0]) < 0);
#endif
	assert(moreNode->children[0] == NULL);

	void * retData;

	if (pos <= lessNode->dataCount)
	{
		_InsertNodeAndData(tree, lessNode, pos, newNode, data);
		lessNode->dataCount--;
		retData = lessNode->data[lessNode->dataCount];
		lessNode->data[lessNode->dataCount] = NULL;
		moreNode->children[0] = 
			lessNode->children[lessNode->dataCount+1];
		if (moreNode->children[0])
		{
			moreNode->children[0]->parent = moreNode;
			moreNode->children[0]->parentPos = 0;
		}
		lessNode->children[lessNode->dataCount+1] = NULL;
	}
	else
	{
		pos -= lessNode->dataCount;
		retData = moreNode->data[0];
		assert(!moreNode->children[0]);
		assert(!moreNode->data[moreNode->dataCount]);

		int j = 0;
		for (; j < pos; j++)
		{
			moreNode->data[j] = moreNode->data[j+1];
			moreNode->children[j] = moreNode->children[j+1];
			if (moreNode->children[j])
			{
				moreNode->children[j]->parentPos = j;
			}
		}

		assert(!moreNode->children[pos-1] ||
		       tree->compareData(data,
			       		 moreNode->children[pos-1]->data[0]) > 0);

		moreNode->data[pos-1] = data;
		moreNode->children[pos] = newNode;
		if (newNode)
		{
			newNode->parent = moreNode;
			newNode->parentPos = pos;
		}
	}

	return retData;
}

static
void
_InsertAt(TreeIterator * iter, void * data)
{
	TreeNode * node = iter->node;
	TreeNode * insertNode = NULL;
	int pos = iter->which;
	
	while (node != NULL)
	{
#ifdef TREE_DEBUG
		assert((pos == node->dataCount ||
		       iter->tree->compareData(data, node->data[pos]) < 0));
#endif

		// see if there's any NULL data in the current node
		if (node->dataCount == DATA_PER_NODE)
		{
			// no open data slots, split this node!
			TreeNode * newNode = _SplitNode(node);

			// insert data in split nodes
			data = _AddDataToSplitNodes(iter->tree,
						    node, 
						    newNode,
						    pos,
						    insertNode,
						    data);

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
				iter->tree->rootNode->data[0] = data;
				iter->tree->rootNode->dataCount = 1;
				return;
			}

			pos = node->parentPos;
			node = node->parent;
			insertNode = newNode;
		}
		else
		{
			// insert the data and newNode
			_InsertNodeAndData(iter->tree, 
					   node,
					   pos,
					   insertNode,
					   data);
			return;
		}
	}
}

static
void
_MergeNodes(TreeNode * lessNode, TreeNode * moreNode)
{
	int i = 0;
	int j = lessNode->dataCount;

	assert((lessNode->dataCount + moreNode->dataCount) <= DATA_PER_NODE);
	assert(lessNode->children[j] == NULL);

	for(; i < moreNode->dataCount; i++,j++)
	{
		assert(!lessNode->children[j]);
		assert(!lessNode->data[j]);
		lessNode->data[j] = moreNode->data[i];
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
	lessNode->dataCount += i;

	free(moreNode);
}

void
_DeleteAt(TreeIterator * iter)
{
	TreeNode * node = iter->node;
	int pos = iter->which;
	void ** data = &(node->data[pos]);
	void * dataToFree = *data;

	{
		// find the least greater than data to fill the whole!
		if (node->children[pos+1])
		{
			TreeNode * child = node->children[++pos];
			while (child->children[0])
			{
				pos = 0;
				child = child->children[0];
			}

			*data = child->data[0];
			data = &(child->data[0]);
			node = child;
		}
		// or the greatest lesser than data to fill the whole!
		else if (node->children[pos])
		{
			TreeNode * child = node->children[pos];
			while (child->children[child->dataCount])
			{
				pos = child->dataCount;
				child = child->children[child->dataCount];
			}

			*data = child->data[child->dataCount-1];
			data = &(child->data[child->dataCount-1]);
			node = child;
		}
		else
		{
			pos = node->parentPos;
		}
	}

	// move data nodes over, we're at a leaf node, so we can ignore
	// children
	int i = data - node->data;;
	for (; i < node->dataCount-1; i++)
	{
		node->data[i] = node->data[i+1];
	}
	node->data[--node->dataCount] = NULL;

	// merge the nodes from the bottom up which have too few data
	while (node->dataCount < (DATA_PER_NODE/2))
	{
		// if we're not the root
		if (node->parent)
		{
			TreeNode ** child = &(node->parent->children[pos]);
			assert(node->parent->children[pos] == node);

			// check siblings for extra data
			if (pos < node->parent->dataCount &&
			    (*(child+1))->dataCount > (DATA_PER_NODE/2))
			{
				child++;
				node->data[node->dataCount++] = 
					node->parent->data[pos];
				node->children[node->dataCount] =
					(*child)->children[0];
				if (node->children[node->dataCount])
				{
					node->children[node->dataCount]->
						parent = node;
					node->children[node->dataCount]->
						parentPos = node->dataCount;
				}
				node->parent->data[pos] =
					(*child)->data[0];
				int i = 0;
				for(; i < (*child)->dataCount-1; i++)
				{
					(*child)->data[i] = (*child)->data[i+1];
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
				(*child)->data[i] = NULL;
				(*child)->dataCount--;
			}
			else if (pos > 0 &&
				 (*(child-1))->dataCount>(DATA_PER_NODE/2))
			{
				child--;
				int i = node->dataCount++;
				for(; i > 0; i--)
				{
					node->data[i] = node->data[i-1];
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
				node->data[0] = node->parent->data[pos-1];
				node->children[0] = 
					(*child)->children[(*child)->dataCount];
				if (node->children[0])
				{
					node->children[0]->parent = node;
					node->children[0]->parentPos = 0;
				}
				node->parent->data[pos-1] = 
					(*child)->data[(*child)->dataCount-1];
				(*child)->children[(*child)->dataCount--] =
					NULL;
				(*child)->data[(*child)->dataCount] = NULL;
			}
			// merge with one of our siblings
			else
			{
				if (pos < node->parent->dataCount)
				{
					child++;
					assert(*child);

					node->data[node->dataCount++] =
						node->parent->data[pos];

					_MergeNodes(node, *child);
				}
				else
				{
					assert(pos > 0);
					child--;
					assert(*child);
					pos--;

					(*child)->data[(*child)->dataCount++] = 
						node->parent->data[pos];

					_MergeNodes(*child, node);
					node = *child;
				}

				int i = pos;
				for(; i < node->parent->dataCount-1; i++)
				{
					node->parent->data[i] =
						node->parent->data[i+1];
					node->parent->children[i+1] =
						node->parent->children[i+2];
					if (node->parent->children[i+1])
					{
						node->parent->children[i+1]->
							parentPos = i+1;
					}
				}
				node->parent->data[i] = NULL;
				node->parent->children[i+1] = NULL;
				node->parent->dataCount--;

				node = node->parent;
				pos = node->parentPos;
			}
		}
		// this is a root node
		else 
		{
			if (node->dataCount == 0)
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

	if (iter->tree->freeData)
	{
		iter->tree->freeData(dataToFree);
	}
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

int
DeleteFromTree(Tree * tree, void * data)
{
	TreeIterator iter;
	_SetTreeIteratorToRoot(&iter, tree);

	if (_Find(&iter, data))
	{
		_DeleteAt(&iter);
		return 1;
	}

	return 0;
}
