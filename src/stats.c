/* the Music Player Daemon (MPD)
 * (c)2003-2004 by Warren Dukes (shank@mercury.chem.pitt.edu)
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

#include "command.h"
#include "tables.h"
#include "directory.h"
#include "myfprintf.h"
#include "player.h"

#include <time.h>

Stats stats;

void initStats() {
	stats.daemonStart = time(NULL);
	stats.numberOfSongs = 0;
	/*stats.playTime = 0;
	stats.songsPlayed = 0;*/
}

int printStats(FILE * fp) {
	myfprintf(fp,"artists: %li\n",numberOfArtists());
	myfprintf(fp,"albums: %li\n",numberOfAlbums());
	myfprintf(fp,"songs: %i\n",stats.numberOfSongs);
	myfprintf(fp,"uptime: %li\n",time(NULL)-stats.daemonStart);
	myfprintf(fp,"playtime: %li\n",(long)(getPlayerTotalPlayTime()+0.5));
	/*myfprintf(fp,"songs_played: %li\n",stats.songsPlayed);*/
	myfprintf(fp,"db_update: %li\n",getDbModTime());
	return 0;
}
