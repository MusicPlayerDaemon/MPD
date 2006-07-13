/* the Music Player Daemon (MPD)
 * (c)2003-2006 by Warren Dukes (shank@mercury.chem.pitt.edu)
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
