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

#include "tag.h"
#include "utils.h"
#include "myfprintf.h"
#include "directory.h"

struct visited {
	struct visited *next;

	/**
	 * this is the original pointer passed to visitInTagTracker(),
	 * i.e. the caller must not invalidate it until he calls
	 * resetVisitedFlagsInTagTracker().
	 */
	const char *value;
} mpd_packed;

static struct visited *visited_heads[TAG_NUM_OF_ITEM_TYPES];
static unsigned num_visited[TAG_NUM_OF_ITEM_TYPES];

static int visit_tag_items(int fd mpd_unused, Song *song, void *data)
{
	enum tag_type type = (enum tag_type)(size_t)data;
	unsigned i;

	if (song->tag == NULL)
		return 0;

	for (i = 0; i < (unsigned)song->tag->numOfItems; ++i) {
		const struct tag_item *item = song->tag->items[i];
		if (item->type == type)
			visitInTagTracker(type, item->value);
	}

	return 0;
}

int getNumberOfTagItems(int type)
{
	int ret;

	resetVisitedFlagsInTagTracker(type);

	traverseAllIn(-1, NULL, visit_tag_items, NULL, (void*)(size_t)type);

	ret = (int)num_visited[type];
	resetVisitedFlagsInTagTracker(type);
	return ret;
}

void resetVisitedFlagsInTagTracker(int type)
{
	while (visited_heads[type] != NULL) {
		struct visited *v = visited_heads[type];
		visited_heads[type] = v->next;
		free(v);
	}

	num_visited[type] = 0;
}

static struct visited *
find_visit(int type, const char *p)
{
	struct visited *v;

	for (v = visited_heads[type]; v != NULL; v = v->next)
		if (strcmp(v->value, p) == 0)
			return v;

	return NULL;
}

void visitInTagTracker(int type, const char *str)
{
	struct visited *v = find_visit(type, str);
	size_t length;

	if (v != NULL)
		return;

	length = strlen(str);
	v = xmalloc(sizeof(*v));
	v->value = str;
	v->next = visited_heads[type];
	visited_heads[type] = v;
	++num_visited[type];
}

void printVisitedInTagTracker(int fd, int type)
{
	struct visited *v;

	for (v = visited_heads[type]; v != NULL; v = v->next)
		fdprintf(fd,
			 "%s: %s\n",
			 mpdTagItemKeys[type],
			 v->value);
}
