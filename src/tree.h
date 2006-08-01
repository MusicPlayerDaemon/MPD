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

int DeleteFromTree(Tree * tree, void * data);

#endif
