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

#ifndef DIRECTORY_H
#define DIRECTORY_H

#include "../config.h"

#include "song.h"
#include "list.h"

typedef List DirectoryList;

typedef struct _DirectoryStat {
	ino_t inode;
	dev_t device;
} DirectoryStat;

typedef struct _Directory {
	char *path;
	DirectoryList *subDirectories;
	SongList *songs;
	struct _Directory *parent;
	DirectoryStat *stat;
} Directory;

void readDirectoryDBIfUpdateIsFinished(void);

int isUpdatingDB(void);

void directory_sigChldHandler(int pid, int status);

int updateInit(int fd, List * pathList);

void initMp3Directory(void);

void closeMp3Directory(void);

int isRootDirectory(char *name);

int printDirectoryInfo(int fd, char *dirname);

int checkDirectoryDB(void);

int writeDirectoryDB(void);

int readDirectoryDB(void);

void updateMp3Directory(void);

Song *getSongFromDB(char *file);

time_t getDbModTime(void);

int traverseAllIn(int fd, char *name,
		  int (*forEachSong) (int, Song *, void *),
		  int (*forEachDir) (int, Directory *, void *), void *data);

#define getDirectoryPath(dir) ((dir && dir->path) ? dir->path : "")

#endif
