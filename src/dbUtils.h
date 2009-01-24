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

#ifndef MPD_DB_UTILS_H
#define MPD_DB_UTILS_H

struct client;
struct locate_item_list;

int printAllIn(struct client *client, const char *name);

int addAllIn(const char *name);

int addAllInToStoredPlaylist(const char *name, const char *utf8file);

int printInfoForAllIn(struct client *client, const char *name);

int
searchForSongsIn(struct client *client, const char *name,
		 const struct locate_item_list *criteria);

int
findSongsIn(struct client *client, const char *name,
	    const struct locate_item_list *criteria);

int
searchStatsForSongsIn(struct client *client, const char *name,
		      const struct locate_item_list *criteria);

unsigned long sumSongTimesIn(const char *name);

int
listAllUniqueTags(struct client *client, int type,
		  const struct locate_item_list *criteria);

void printSavedMemoryFromFilenames(void);

#endif
