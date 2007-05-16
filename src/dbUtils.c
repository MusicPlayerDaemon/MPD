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

#include "dbUtils.h"

#include "directory.h"
#include "myfprintf.h"
#include "utils.h"
#include "playlist.h"
#include "song.h"
#include "tag.h"
#include "tagTracker.h"
#include "log.h"
#include "storedPlaylist.h"

typedef struct _ListCommandItem {
	mpd_sint8 tagType;
	int numConditionals;
	LocateTagItem *conditionals;
} ListCommandItem;

typedef struct _LocateTagItemArray {
	int numItems;
	LocateTagItem *items;
} LocateTagItemArray;

typedef struct _SearchStats {
	LocateTagItemArray locateArray;
	int numberOfSongs;
	unsigned long playTime;
} SearchStats;

static int countSongsInDirectory(int fd, Directory * directory, void *data)
{
	int *count = (int *)data;

	*count += directory->songs->numberOfNodes;

	return 0;
}

static int printDirectoryInDirectory(int fd, Directory * directory,
				     void *data)
{
	if (directory->path) {
		fdprintf(fd, "directory: %s\n", getDirectoryPath(directory));
	}
	return 0;
}

static int printSongInDirectory(int fd, Song * song, void *data)
{
	printSongUrl(fd, song);
	return 0;
}

static int searchInDirectory(int fd, Song * song, void *data)
{
	LocateTagItemArray *array = data;

	if (strstrSearchTags(song, array->numItems, array->items))
		printSongInfo(fd, song);

	return 0;
}

int searchForSongsIn(int fd, char *name, int numItems, LocateTagItem * items)
{
	int ret = -1;
	int i;

	char **originalNeedles = xmalloc(numItems * sizeof(char *));
	LocateTagItemArray array;

	for (i = 0; i < numItems; i++) {
		originalNeedles[i] = items[i].needle;
		items[i].needle = strDupToUpper(originalNeedles[i]);
	}

	array.numItems = numItems;
	array.items = items;

	ret = traverseAllIn(fd, name, searchInDirectory, NULL, &array);

	for (i = 0; i < numItems; i++) {
		free(items[i].needle);
		items[i].needle = originalNeedles[i];
	}

	free(originalNeedles);

	return ret;
}

static int findInDirectory(int fd, Song * song, void *data)
{
	LocateTagItemArray *array = data;

	if (tagItemsFoundAndMatches(song, array->numItems, array->items))
		printSongInfo(fd, song);

	return 0;
}

int findSongsIn(int fd, char *name, int numItems, LocateTagItem * items)
{
	LocateTagItemArray array;

	array.numItems = numItems;
	array.items = items;

	return traverseAllIn(fd, name, findInDirectory, NULL, (void *)&array);
}

static void printSearchStats(int fd, SearchStats *stats)
{
	fdprintf(fd, "songs: %i\n", stats->numberOfSongs);
	fdprintf(fd, "playtime: %li\n", stats->playTime);
}

static int searchStatsInDirectory(int fd, Song * song, void *data)
{
	SearchStats *stats = data;

	if (tagItemsFoundAndMatches(song, stats->locateArray.numItems,
	                                  stats->locateArray.items)) {
		stats->numberOfSongs++;
		if (song->tag->time > 0)
			stats->playTime += song->tag->time;
	}

	return 0;
}

int searchStatsForSongsIn(int fd, char *name, int numItems,
                          LocateTagItem * items)
{
	SearchStats stats;
	int ret;

	stats.locateArray.numItems = numItems;
	stats.locateArray.items = items;
	stats.numberOfSongs = 0;
	stats.playTime = 0;

	ret = traverseAllIn(fd, name, searchStatsInDirectory, NULL, &stats);
	if (ret == 0)
		printSearchStats(fd, &stats);

	return ret;
}

int printAllIn(int fd, char *name)
{
	return traverseAllIn(fd, name, printSongInDirectory,
			     printDirectoryInDirectory, NULL);
}

static int directoryAddSongToPlaylist(int fd, Song * song, void *data)
{
	return addSongToPlaylist(fd, song, 0);
}

static int directoryAddSongToStoredPlaylist(int fd, Song *song, void *data)
{
	if (appendSongToStoredPlaylistByPath(fd, (char *)data, song) != 0)
		return -1;
	return 0;
}

int addAllIn(int fd, char *name)
{
	return traverseAllIn(fd, name, directoryAddSongToPlaylist, NULL, NULL);
}

int addAllInToStoredPlaylist(int fd, char *name, char *utf8file)
{
	return traverseAllIn(fd, name, directoryAddSongToStoredPlaylist, NULL,
	                     (void *)utf8file);
}

static int directoryPrintSongInfo(int fd, Song * song, void *data)
{
	return printSongInfo(fd, song);
}

static int sumSongTime(int fd, Song * song, void *data)
{
	unsigned long *time = (unsigned long *)data;

	if (song->tag && song->tag->time >= 0)
		*time += song->tag->time;

	return 0;
}

int printInfoForAllIn(int fd, char *name)
{
	return traverseAllIn(fd, name, directoryPrintSongInfo,
			     printDirectoryInDirectory, NULL);
}

int countSongsIn(int fd, char *name)
{
	int count = 0;
	void *ptr = (void *)&count;

	traverseAllIn(fd, name, NULL, countSongsInDirectory, ptr);

	return count;
}

unsigned long sumSongTimesIn(int fd, char *name)
{
	unsigned long dbPlayTime = 0;
	void *ptr = (void *)&dbPlayTime;

	traverseAllIn(fd, name, sumSongTime, NULL, ptr);

	return dbPlayTime;
}

static ListCommandItem *newListCommandItem(int tagType, int numConditionals,
					   LocateTagItem * conditionals)
{
	ListCommandItem *item = xmalloc(sizeof(ListCommandItem));

	item->tagType = tagType;
	item->numConditionals = numConditionals;
	item->conditionals = conditionals;

	return item;
}

static void freeListCommandItem(ListCommandItem * item)
{
	free(item);
}

static void visitTag(int fd, Song * song, int tagType)
{
	int i;
	MpdTag *tag = song->tag;

	if (tagType == LOCATE_TAG_FILE_TYPE) {
		printSongUrl(fd, song);
		return;
	}

	if (!tag)
		return;

	for (i = 0; i < tag->numOfItems; i++) {
		if (tag->items[i].type == tagType) {
			visitInTagTracker(tagType, tag->items[i].value);
		}
	}
}

static int listUniqueTagsInDirectory(int fd, Song * song, void *data)
{
	ListCommandItem *item = data;

	if (tagItemsFoundAndMatches(song, item->numConditionals,
	                            item->conditionals)) {
		visitTag(fd, song, item->tagType);
	}

	return 0;
}

int listAllUniqueTags(int fd, int type, int numConditionals,
		      LocateTagItem * conditionals)
{
	int ret;
	ListCommandItem *item = newListCommandItem(type, numConditionals,
						   conditionals);

	if (type >= 0 && type <= TAG_NUM_OF_ITEM_TYPES) {
		resetVisitedFlagsInTagTracker(type);
	}

	ret = traverseAllIn(fd, NULL, listUniqueTagsInDirectory, NULL,
			    (void *)item);

	if (type >= 0 && type <= TAG_NUM_OF_ITEM_TYPES) {
		printVisitedInTagTracker(fd, type);
	}

	freeListCommandItem(item);

	return ret;
}

static int sumSavedFilenameMemoryInDirectory(int fd, Directory * dir,
					     void *data)
{
	int *sum = data;

	if (!dir->path)
		return 0;

	*sum += (strlen(getDirectoryPath(dir)) + 1 - sizeof(Directory *)) *
	    dir->songs->numberOfNodes;

	return 0;
}

static int sumSavedFilenameMemoryInSong(int fd, Song * song, void *data)
{
	int *sum = data;

	*sum += strlen(song->url) + 1;

	return 0;
}

void printSavedMemoryFromFilenames(void)
{
	int sum = 0;

	traverseAllIn(STDERR_FILENO, NULL, sumSavedFilenameMemoryInSong,
		      sumSavedFilenameMemoryInDirectory, (void *)&sum);

	DEBUG("saved memory from filenames: %i\n", sum);
}
