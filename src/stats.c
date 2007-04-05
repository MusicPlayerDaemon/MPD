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

#include "stats.h"

#include "directory.h"
#include "myfprintf.h"
#include "player.h"
#include "tag.h"
#include "tagTracker.h"

#include <time.h>

Stats stats;

void initStats(void)
{
	stats.daemonStart = time(NULL);
	stats.numberOfSongs = 0;
}

int printStats(int fd)
{
	fdprintf(fd, "artists: %i\n", getNumberOfTagItems(TAG_ITEM_ARTIST));
	fdprintf(fd, "albums: %i\n", getNumberOfTagItems(TAG_ITEM_ALBUM));
	fdprintf(fd, "songs: %i\n", stats.numberOfSongs);
	fdprintf(fd, "uptime: %li\n", time(NULL) - stats.daemonStart);
	fdprintf(fd, "playtime: %li\n",
		  (long)(getPlayerTotalPlayTime() + 0.5));
	fdprintf(fd, "db_playtime: %li\n", stats.dbPlayTime);
	fdprintf(fd, "db_update: %li\n", getDbModTime());
	return 0;
}
