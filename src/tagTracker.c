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

#include "tagTracker.h"

#include "log.h"

#include <glib/gtree.h>
#include <assert.h>
#include <stdlib.h>

static GTree * tagLists[TAG_NUM_OF_ITEM_TYPES] =
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

int keyCompare(const void *a, const void *b, void *data) {
	return strcmp(a,b);
}

char * getTagItemString(int type, char * string) {
	TagTrackerItem * item;
	TagTrackerItem ** itemPointer = &item;
	char *key;
	char **keyPointer = &key;
	

	/*if(type == TAG_ITEM_TITLE) return strdup(string);*/

	if(tagLists[type] == NULL) {
		tagLists[type] = g_tree_new_full(keyCompare, NULL, free, free);
	}

	if((TagTrackerItem *)g_tree_lookup_extended(tagLists[type], string, (void**)keyPointer, (void**)itemPointer )) {
		item->count++;
	}
	else {
		item = malloc(sizeof(TagTrackerItem));
		item->count = 1;
		item->visited = 0;
		key = strdup(string);
		g_tree_insert(tagLists[type], key, item);
		
				
	}

	return key;
}


void removeTagItemString(int type, char * string) {
	TagTrackerItem *item;

	assert(string);

	assert(tagLists[type]);
	if(tagLists[type] == NULL) return;

	if((item = g_tree_lookup(tagLists[type], string))) {
		item->count--;
		if(item->count <= 0) g_tree_remove(tagLists[type], string);
	}

/* why would this be done??? free it when mpd quits...
 	if(tagLists[type]->numberOfNodes == 0) {
		freeList(tagLists[type]);
		tagLists[type] = NULL;
	}
*/
}

void destroyTagTracker() {
	int type;
	for (type=0; type < TAG_NUM_OF_ITEM_TYPES; type ++) 
		if (tagLists[type])
			g_tree_destroy(tagLists[type]);
}

int getNumberOfTagItems(int type) {
	if(tagLists[type] == NULL) return 0;

	return g_tree_nnodes(tagLists[type]);
}
int calcSavedMemory(char *key, TagTrackerItem* value, int* sum) {
	*sum -= sizeof(int) + 4*sizeof(void*); /* sizeof(_GTreeNode) */
	*sum -= sizeof(TagTrackerItem);
	*sum += (strlen(key)+1)*value->count;
	return FALSE;
}
	
void printMemorySavedByTagTracker() {
	int i;
	size_t sum = 0;

	for(i = 0; i < TAG_NUM_OF_ITEM_TYPES; i++) {
		if(!tagLists[i]) continue;

		sum -= 5*sizeof(void*);/* sizeof(_GTree) */
		g_tree_foreach(tagLists[i], (GTraverseFunc)calcSavedMemory, &sum);
	}

	DEBUG("saved memory from tags: %li\n", (long)sum);
}

void sortTagTrackerInfo() {
	/* implicit sorting
	int i;

	for(i = 0; i < TAG_NUM_OF_ITEM_TYPES; i++) {
		if(!tagLists[i]) continue;

		DEBUG("sorting %s info\n", mpdTagItemKeys[i]);

		sortList(tagLists[i]);
	}*/
}

int resetVisitedFlag(char *key, TagTrackerItem *value, void *data) {
	value->visited = 0;
	return FALSE;
}
void resetVisitedFlagsInTagTracker(int type) {

	if(!tagLists[type]) return;

	g_tree_foreach(tagLists[type], (GTraverseFunc)resetVisitedFlag, NULL);
}

int wasVisitedInTagTracker(int type, char * str) {
	TagTrackerItem * item;

	if(!tagLists[type]) return 0;

	if(!(item = g_tree_lookup(tagLists[type], str))) return 0;

	return item->visited;
}

void visitInTagTracker(int type, char * str) {
	TagTrackerItem * item;

	if(!tagLists[type]) return;

	if(!(item = g_tree_lookup(tagLists[type], str))) return;

	item->visited = 1;
}

struct _PrintVisitedUserdata {
	FILE *fp;
	char *type;
};

int printVisitedFlag(char *key, TagTrackerItem* value, struct _PrintVisitedUserdata *data) {
	if(value->visited) myfprintf(data->fp, "%s: %s\n", data->type, key);
	return FALSE;
}

void printVisitedInTagTracker(FILE * fp, int type) {
	struct _PrintVisitedUserdata data = {fp, mpdTagItemKeys[type]};
	if(!tagLists[type]) return;
	g_tree_foreach( tagLists[type], (GTraverseFunc)printVisitedFlag, (void*)&data);
}
