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

#ifndef LS_H
#define LS_H

#include "../config.h"

#include <stdio.h>
#include <time.h>

int lsPlaylists(FILE * fp, char * utf8path);

int isMp3(char * utf8file, time_t * mtime);

int isMp4(char * utf8file, time_t * mtime);

int isOgg(char * utf8file, time_t * mtime);

int isFlac(char * utf8file, time_t * mtime);

int isWave(char * utf8file, time_t * mtime);

int isMusic(char * utf8file, time_t * mtime);

int isDir(char * utf8name, time_t * mtime);

int isPlaylist(char * utf8file);

char * dupAndStripPlaylistSuffix(char * file);

#endif
