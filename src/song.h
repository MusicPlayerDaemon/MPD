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

#define SONG_BEGIN	"songList begin"
#define SONG_END	"songList end"

#include <sys/param.h>
#include <time.h>

#include "tag.h"
#include "list.h"

typedef struct _Song {
	char * utf8file;
	MpdTag * tag;
	time_t mtime;
} Song;

typedef List SongList;

Song * newSong(char * utf8file);

void freeSong(Song *);

SongList * newSongList();

void freeSongList(SongList * list);

Song * addSongToList(SongList * list, char * key, char * utf8file);

int printSongInfo(FILE * fp, Song * song);

int printSongInfoFromList(FILE * fp, SongList * list);

void writeSongInfoFromList(FILE * fp, SongList * list);

void readSongInfoIntoList(FILE * fp, SongList * list);

int updateSongInfo(Song * song);

Song * songDup(Song * song);

#endif
