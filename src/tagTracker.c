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

	if(type == TAG_ITEM_TITLE) return strdup(string);
	
	if(tagLists[type] == NULL) {
		tagLists[type] = makeList(free);
	}

	if((node = findNodeInList(tagLists[type], string))) {
		((TagTrackerItem *)node->data)->count++;
	}
	else {
		TagTrackerItem * item = malloc(sizeof(TagTrackerItem));
		item->count = 1;
		item->visited = 0;
		node = insertInList(tagLists[type], string, item);
	}

	return node->key;
}

void removeTagItemString(int type, char * string) {
	ListNode * node;

	assert(string);

	if(type == TAG_ITEM_TITLE) {
		free(string);
		return;
	}
	
	assert(tagLists[type]);
	if(tagLists[type] == NULL) return;

	node = findNodeInList(tagLists[type], string);
	assert(node);
	if(node) {
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

	DEBUG("saved memory: %li\n", (long)sum);
}

void sortTagTrackerInfo() {
	int i;

	for(i = 0; i < TAG_NUM_OF_ITEM_TYPES; i++) {
		if(!tagLists[i]) continue;

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
	int ret;
	ListNode * node;
	TagTrackerItem * item;

	if(!tagLists[type]) return 0;

	node = findNodeInList(tagLists[type], str);

	if(!node) return 0;

	item = node->data;
	ret = item->visited;
	item->visited = 1;

	return ret;
}
