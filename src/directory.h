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

#ifndef DIRECTORY_H
#define DIRECTORY_H

#include "../config.h"

#include "song.h"
#include "list.h"

#include <stdio.h>
#include <sys/param.h>

extern char * directory_db;

void readDirectoryDBIfUpdateIsFinished();

void clearUpdatePid();

int isUpdatingDB();

void directory_sigChldHandler(int pid, int status);

int updateInit(FILE * fp, List * pathList);

void initMp3Directory();

void closeMp3Directory();

int printDirectoryInfo(FILE * fp, char * dirname);

int writeDirectoryDB();

int readDirectoryDB();

int updateMp3Directory(FILE * fp);

int printAllIn(FILE * fp, char * name);

int addAllIn(FILE * fp, char * name);

int printInfoForAllIn(FILE * fp, char * name);

int searchForSongsIn(FILE * fp, char * name, char * item, char * string);

int findSongsIn(FILE * fp, char * name, char * item, char * string);

int countSongsIn(FILE * fp, char * name);

unsigned long sumSongTimesIn(FILE * fp, char * name);

Song * getSongFromDB(char * file);

time_t getDbModTime();

#endif
/* vim:set shiftwidth=4 tabstop=8 expandtab: */
