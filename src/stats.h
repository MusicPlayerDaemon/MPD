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

#ifndef MPD_STATS_H
#define MPD_STATS_H

#include <glib.h>

struct client;

struct stats {
	GTimer *timer;

	/** number of song files in the music directory */
	unsigned song_count;

	/** sum of all song durations in the music directory (in
	    seconds) */
	unsigned long song_duration;

	/** number of distinct artist names in the music directory */
	unsigned artist_count;

	/** number of distinct album names in the music directory */
	unsigned album_count;
};

extern struct stats stats;

void stats_global_init(void);

void stats_global_finish(void);

void stats_update(void);

int stats_print(struct client *client);

#endif
