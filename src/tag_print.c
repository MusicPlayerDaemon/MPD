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

#include "tag_print.h"
#include "tag.h"
#include "tag_internal.h"
#include "myfprintf.h"
#include "song.h"

void tag_print_types(int fd)
{
	int i;

	for (i = 0; i < TAG_NUM_OF_ITEM_TYPES; i++) {
		if (ignoreTagItems[i] == 0)
			fdprintf(fd, "tagtype: %s\n", mpdTagItemKeys[i]);
	}
}

void tag_print(int fd, const struct tag *tag)
{
	int i;

	if (tag->time >= 0)
		fdprintf(fd, SONG_TIME "%i\n", tag->time);

	for (i = 0; i < tag->numOfItems; i++) {
		fdprintf(fd, "%s: %s\n", mpdTagItemKeys[tag->items[i]->type],
			  tag->items[i]->value);
	}
}
