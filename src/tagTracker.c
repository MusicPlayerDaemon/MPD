#include "tagTracker.h"

#include "list.h"
#include "log.h"

#include <assert.h>

static List * tagLists[TAG_NUM_OF_ITEM_TYPES] =
{
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

char * getTagItemString(int type, char * string) {
	ListNode * node;

	/*if(type == TAG_ITEM_TITLE) return strdup(string);*/

	if(tagLists[type] == NULL) {
		tagLists[type] = makeList(free, 1);
		sortList(tagLists[type]);
	}

	if(findNodeInList(tagLists[type], string, &node)) {
		DEBUG("found\n");
		((TagTrackerItem *)node->data)->count++;
	}
	else {
		DEBUG("not found\n");
		TagTrackerItem * item = malloc(sizeof(TagTrackerItem));
		item->count = 1;
		item->visited = 0;
		node = insertInListBeforeNode(tagLists[type], node, string, 
				item);
	}

	DEBUG("key: %s:%s\n", string, node->key);
	return node->key;
}

void removeTagItemString(int type, char * string) {
	ListNode * node;

	assert(string);

	assert(tagLists[type]);
	if(tagLists[type] == NULL) return;

	/*if(!node) {
		free(string);
		return;
	}*/

	if(findNodeInList(tagLists[type], string, &node)) {
		TagTrackerItem * item = node->data;
		item->count--;
		if(item->count <= 0) deleteNodeFromList(tagLists[type], node);
	}

	if(tagLists[type]->numberOfNodes == 0) {
		freeList(tagLists[type]);
		tagLists[type] = NULL;
	}
}

int getNumberOfTagItems(int type) {
	if(tagLists[type] == NULL) return 0;

	return tagLists[type]->numberOfNodes;
}

void printMemorySavedByTagTracker() {
	int i;
	ListNode * node;
	size_t sum = 0;

	for(i = 0; i < TAG_NUM_OF_ITEM_TYPES; i++) {
		if(!tagLists[i]) continue;

		sum -= sizeof(List);
		
		node = tagLists[i]->firstNode;

		while(node != NULL) {
			sum -= sizeof(ListNode);
			sum -= sizeof(TagTrackerItem);
			sum -= sizeof(node->key);
			sum += (strlen(node->key)+1)*(*((int *)node->data));
			node = node->nextNode;
		}
	}

	DEBUG("saved memory from tags: %li\n", (long)sum);
}

void sortTagTrackerInfo() {
	int i;

	for(i = 0; i < TAG_NUM_OF_ITEM_TYPES; i++) {
		if(!tagLists[i]) continue;

		DEBUG("sorting %s info\n", mpdTagItemKeys[i]);

		sortList(tagLists[i]);
	}
}

void resetVisitedFlagsInTagTracker(int type) {
	ListNode * node;

	if(!tagLists[type]) return;

	node = tagLists[type]->firstNode;

	while(node) {
		((TagTrackerItem *)node->data)->visited = 0;
		node = node->nextNode;
	}
}

int wasVisitedInTagTracker(int type, char * str) {
	ListNode * node;

	if(!tagLists[type]) return 0;

	if(!findNodeInList(tagLists[type], str, &node)) return 0;

	return ((TagTrackerItem *)node->data)->visited;
}

void visitInTagTracker(int type, char * str) {
	ListNode * node;

	if(!tagLists[type]) return;

	if(!findNodeInList(tagLists[type], str, &node)) return;

	((TagTrackerItem *)node->data)->visited = 1;
}

void printVisitedInTagTracker(FILE * fp, int type) {
	ListNode * node;
	TagTrackerItem * item;

	if(!tagLists[type]) return;

	node = tagLists[type]->firstNode;

	while(node) {
		item = node->data;
		if(item->visited) {
			myfprintf(fp, "%s: %s\n", mpdTagItemKeys[type],
					node->key);
		}
		node = node->nextNode;
	}
}
