/* the Music Player Daemon (MPD)
 * (c)2003-2006 by Warren Dukes (warren.dukes@gmail.com)
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

#include "tagTracker.h"

#include "list.h"
#include "log.h"

#include <assert.h>

static List *tagLists[TAG_NUM_OF_ITEM_TYPES] = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

typedef struct tagTrackerItem {
	int count;
	mpd_sint8 visited;
} TagTrackerItem;

char *getTagItemString(int type, char *string)
{
	ListNode *node;
	int pos;

	if (tagLists[type] == NULL) {
		tagLists[type] = makeList(free, 1);
		sortList(tagLists[type]);
	}

	if (findNodeInList(tagLists[type], string, &node, &pos)) {
		((TagTrackerItem *) node->data)->count++;
	} else {
		TagTrackerItem *item = malloc(sizeof(TagTrackerItem));
		item->count = 1;
		item->visited = 0;
		node = insertInListBeforeNode(tagLists[type], node, pos,
					      string, item);
	}

	return node->key;
}

void removeTagItemString(int type, char *string)
{
	ListNode *node;
	int pos;

	assert(string);

	assert(tagLists[type]);
	if (tagLists[type] == NULL)
		return;

	if (findNodeInList(tagLists[type], string, &node, &pos)) {
		TagTrackerItem *item = node->data;
		item->count--;
		if (item->count <= 0)
			deleteNodeFromList(tagLists[type], node);
	}

	if (tagLists[type]->numberOfNodes == 0) {
		freeList(tagLists[type]);
		tagLists[type] = NULL;
	}
}

int getNumberOfTagItems(int type)
{
	if (tagLists[type] == NULL)
		return 0;

	return tagLists[type]->numberOfNodes;
}

void printMemorySavedByTagTracker(void)
{
	int i;
	ListNode *node;
	size_t sum = 0;

	for (i = 0; i < TAG_NUM_OF_ITEM_TYPES; i++) {
		if (!tagLists[i])
			continue;

		sum -= sizeof(List);

		node = tagLists[i]->firstNode;

		while (node != NULL) {
			sum -= sizeof(ListNode);
			sum -= sizeof(TagTrackerItem);
			sum -= sizeof(node->key);
			sum += (strlen(node->key) + 1) * (*((int *)node->data));
			node = node->nextNode;
		}
	}

	DEBUG("saved memory from tags: %li\n", (long)sum);
}

void resetVisitedFlagsInTagTracker(int type)
{
	ListNode *node;

	if (!tagLists[type])
		return;

	node = tagLists[type]->firstNode;

	while (node) {
		((TagTrackerItem *) node->data)->visited = 0;
		node = node->nextNode;
	}
}

void visitInTagTracker(int type, char *str)
{
	void *item;

	if (!tagLists[type])
		return;

	if (!findInList(tagLists[type], str, &item))
		return;

	((TagTrackerItem *) item)->visited = 1;
}

void printVisitedInTagTracker(int fd, int type)
{
	ListNode *node;
	TagTrackerItem *item;

	if (!tagLists[type])
		return;

	node = tagLists[type]->firstNode;

	while (node) {
		item = node->data;
		if (item->visited) {
			fdprintf(fd, "%s: %s\n", mpdTagItemKeys[type],
				 node->key);
		}
		node = node->nextNode;
	}
}
