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

#ifndef SONG_H
#define SONG_H

#include "../config.h"

#include <sys/param.h>
#include <time.h>

#include "tag.h"
#include "list.h"

#define SONG_BEGIN	"songList begin"
#define SONG_END	"songList end"

#define SONG_TYPE_FILE 1
#define SONG_TYPE_URL 2

typedef struct _Song {
	char * url;
	mpd_sint8 type;
	MpdTag * tag;
	struct _Directory * parentDir;
	time_t mtime;
} Song;

typedef List SongList;

Song * newNullSong();

Song * newSong(char * url, int songType, struct _Directory * parentDir);

void freeSong(Song *);

void freeJustSong(Song *);

SongList * newSongList();

void freeSongList(SongList * list);

Song * addSongToList(SongList * list, char * url, char * utf8path,
		int songType, struct _Directory * parentDir);

int printSongInfo(FILE * fp, Song * song);

int printSongInfoFromList(FILE * fp, SongList * list);

void writeSongInfoFromList(FILE * fp, SongList * list);

void readSongInfoIntoList(FILE * fp, SongList * list, 
		struct _Directory * parent);

int updateSongInfo(Song * song);

Song * songDup(Song * song);

void printSongUrl(FILE * fp, Song * song);

char * getSongUrl(Song * song);

#endif
