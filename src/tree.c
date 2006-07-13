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
