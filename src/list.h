/* the Music Player Daemon (MPD)
 * (c)2003-2004 by Warren Dukes (shank@mercury.chem.pitt.edu)
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


#ifndef LIST_H
#define LIST_H

#include "../config.h"

#include <stdlib.h>

/* used to make a list where free() will be used to free data in list */
#define DEFAULT_FREE_DATA_FUNC	free

/* typedef for function to free data stored in the list nodes */
typedef void ListFreeDataFunc(void *);

typedef struct _ListNode {
	/* used to identify node (ie. when using findInList) */
	char * key;
	/* data store in node */
	void * data;
	/* next node in list */
	struct _ListNode * nextNode;
	/* previous node in list */
	struct _ListNode * prevNode;
} ListNode;

typedef struct _List {
	/* first node in list */
	ListNode * firstNode;
	/* last node in list */
	ListNode * lastNode;
	/* function used to free data stored in nodes of the list */
	ListFreeDataFunc * freeDataFunc;
	/* number of nodes */
	long numberOfNodes;
	/* array for searching when list is sorted */
	ListNode ** nodesArray;
	/* sorted */
	int sorted;
	/* weather to strdup() key's on insertion */
	int strdupKeys;
} List;

/* allocates memory for a new list and initializes it
 *  _freeDataFunc_ -> pointer to function used to free data, use 
 *                    DEFAULT_FREE_DATAFUNC to use free()
 * returns pointer to new list if successful, NULL otherwise
 */
List * makeList(ListFreeDataFunc * freeDataFunc, int strdupKeys);

/* inserts a node into _list_ with _key_ and _data_
 *  _list_ -> list the data will be inserted in
 *  _key_ -> identifier for node/data to be inserted into list
 *  _data_ -> data to be inserted in list
 * returns 1 if successful, 0 otherwise
 */ 
ListNode * insertInList(List * list,char * key,void * data);

ListNode * insertInListBeforeNode(List * list, ListNode * beforeNode, 
		int pos, char * key, void * data);
 
int insertInListWithoutKey(List * list,void * data);

/* deletes the first node in the list with the key _key_
 *  _list_ -> list the node will be deleted from
 *  _key_ -> key used to identify node to delete
 *  returns 1 if node is found and deleted, 0 otherwise
 */
int deleteFromList(List * list,char * key);

void deleteNodeFromList(List * list,ListNode * node);

/* finds data in a list based on key
 *  _list_ -> list to search for _key_ in
 * _key_ -> which node is being searched for
 * _data_ -> a pointer to where data will be placed, 
 *	_data_ memory should not by allocated or freed
 *      _data_ can be NULL
 * returns 1 if successful, 0 otherwise
 */
int findInList(List * list, char * key, void ** data);

/* if _key_ is not found, *_node_ is assigned to the node before which
	the info would be found */
int findNodeInList(List * list, char * key, ListNode ** node, int * pos);

/* frees memory malloc'd for list and its nodes
 *  _list_ -> List to be free'd
 */
void freeList(void * list);

void clearList(List * list);

void sortList(List * list);

#endif
