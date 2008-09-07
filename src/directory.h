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

#include "song.h"

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

/**
 * Starts the tag cache update in the specified location(s).  Returns
 * the job id on success, -1 on error or 0 if an update is already
 * running.
 */
int updateInit(List * pathList);

void initMp3Directory(void);

void closeMp3Directory(void);

int isRootDirectory(const char *name);

int printDirectoryInfo(struct client *client, const char *dirname);

int checkDirectoryDB(void);

int writeDirectoryDB(void);

int readDirectoryDB(void);

void updateMp3Directory(void);

Song *getSongFromDB(const char *file);

time_t getDbModTime(void);

int traverseAllIn(const char *name,
		  int (*forEachSong) (Song *, void *),
		  int (*forEachDir) (Directory *, void *), void *data);

#define getDirectoryPath(dir) ((dir && dir->path) ? dir->path : "")

#endif
