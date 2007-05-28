/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
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

#include "tree.h"
#include "log.h"
#include "utils.h"
#include "myfprintf.h"

#include <assert.h>
#include <stdlib.h>

static Tree *tagTrees[TAG_NUM_OF_ITEM_TYPES];

typedef struct tagTrackerItem {
	int count;
	mpd_sint8 visited;
} TagTrackerItem;

char *getTagItemString(int type, char *string)
{
	TreeIterator iter;
	
	if (tagTrees[type] == NULL) 
	{
		tagTrees[type] = MakeTree((TreeCompareKeyFunction)strcmp, 
					  (TreeFreeFunction)free, 
					  (TreeFreeFunction)free);
	}

	if (FindInTree(tagTrees[type], string, &iter)) 
	{
		((TagTrackerItem *)GetTreeKeyData(&iter).data)->count++;
		return (char *)GetTreeKeyData(&iter).key;
	} 
	else 
	{
		TagTrackerItem *item = xmalloc(sizeof(TagTrackerItem));
		char *key = xstrdup(string);
		item->count = 1;
		item->visited = 0;
		InsertInTree(tagTrees[type], key, item);
		return key;
	}
}

void removeTagItemString(int type, char *string)
{
	TreeIterator iter;
	
	assert(string);

	assert(tagTrees[type]);
	if (tagTrees[type] == NULL)
		return;

	if (FindInTree(tagTrees[type], string, &iter)) 
	{
		TagTrackerItem * item = 
			(TagTrackerItem *)GetTreeKeyData(&iter).data;
		item->count--;
		if (item->count <= 0)
			RemoveFromTreeByIterator(tagTrees[type], &iter);
	}

	if (GetTreeSize(tagTrees[type]) == 0) 
	{
		FreeTree(tagTrees[type]);
		tagTrees[type] = NULL;
	}
}

int getNumberOfTagItems(int type)
{
	if (tagTrees[type] == NULL)
		return 0;

	return GetTreeSize(tagTrees[type]);
}

void resetVisitedFlagsInTagTracker(int type)
{
	TreeIterator iter;

	if (!tagTrees[type])
		return;

	for (SetTreeIteratorToBegin(tagTrees[type], &iter);
	     !IsTreeIteratorAtEnd(&iter);
	     IncrementTreeIterator(&iter))
	{
		((TagTrackerItem *)GetTreeKeyData(&iter).data)->visited = 0;
	}
}

void visitInTagTracker(int type, char *str)
{
	TreeIterator iter;

	if (!tagTrees[type])
		return;

	if (!FindInTree(tagTrees[type], str, &iter))
		return;

	((TagTrackerItem *)GetTreeKeyData(&iter).data)->visited = 1;
}

void printVisitedInTagTracker(int fd, int type)
{
	TreeIterator iter;
	TagTrackerItem * item;

	if (!tagTrees[type])
		return;

	for (SetTreeIteratorToBegin(tagTrees[type], &iter);
	     !IsTreeIteratorAtEnd(&iter);
	     IncrementTreeIterator(&iter))
	{
		item = ((TagTrackerItem *)GetTreeKeyData(&iter).data);

		if (item->visited) 
		{
			fdprintf(fd, 
				 "%s: %s\n", 
				 mpdTagItemKeys[type],
				 (char *)GetTreeKeyData(&iter).key);
		}
	}
}
