/* the Music Player Daemon (MPD)
 * (c)2007 by Warren Dukes (warren.dukes@gmail.com)
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

#ifndef MPD_LOCATE_H
#define MPD_LOCATE_H

#include <stdint.h>
#include <stdbool.h>

#define LOCATE_TAG_FILE_TYPE	TAG_NUM_OF_ITEM_TYPES+10
#define LOCATE_TAG_ANY_TYPE     TAG_NUM_OF_ITEM_TYPES+20

struct song;

/* struct used for search, find, list queries */
struct locate_item {
	int8_t tag;
	/* what we are looking for */
	char *needle;
};

int
locate_parse_type(const char *str);

/* returns NULL if not a known type */
struct locate_item *
locate_item_new(const char *type_string, const char *needle);

/* return number of items or -1 on error */
int
locate_item_list_parse(char *argv[], int argc, struct locate_item **arrayRet);

void
locate_item_list_free(int count, struct locate_item *array);

void
locate_item_free(struct locate_item *item);

bool
locate_song_search(const struct song *song, int numItems,
		   const struct locate_item *items);

bool
locate_song_match(const struct song *song, int numItems,
		  const struct locate_item *items);

#endif
