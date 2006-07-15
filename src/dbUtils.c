/* the Music Player Daemon (MPD)
 * (c)2003-2006 by Warren Dukes (warren.dukes@gmail.com)
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
#include "tag.h"
#include "tagTracker.h"
#include "log.h"

#define LOCATE_TAG_FILE_TYPE	TAG_NUM_OF_ITEM_TYPES+10
#define LOCATE_TAG_FILE_KEY	"filename"
#define LOCATE_TAG_ANY_TYPE     TAG_NUM_OF_ITEM_TYPES+20
#define LOCATE_TAG_ANY_KEY      "any"


typedef struct _ListCommandItem {
	mpd_sint8 tagType;
	int numConditionals;
	LocateTagItem * conditionals;
} ListCommandItem;

typedef struct _LocateTagItemArray {
	int numItems;
	LocateTagItem * items;
} LocateTagItemArray;

int getLocateTagItemType(char * str) {
	int i;

	if(0 == strcasecmp(str, LOCATE_TAG_FILE_KEY)) {
		return LOCATE_TAG_FILE_TYPE;
	}

	if(0 == strcasecmp(str, LOCATE_TAG_ANY_KEY)) {
	  	return LOCATE_TAG_ANY_TYPE;
	}
	
	for(i = 0; i < TAG_NUM_OF_ITEM_TYPES; i++) {
		if(0 == strcasecmp(str, mpdTagItemKeys[i])) return i;
	}

	return -1;
}

static int initLocateTagItem(LocateTagItem * item, char * typeStr, 
		char * needle)
{
	item->tagType = getLocateTagItemType(typeStr);

	if(item->tagType < 0) return -1;

	item->needle = strdup(needle);

	return 0;
}

LocateTagItem * newLocateTagItem(char * typeStr, char * needle) {
	LocateTagItem * ret = malloc(sizeof(LocateTagItem));

	if(initLocateTagItem(ret, typeStr, needle) < 0) {
		free(ret);
		ret = NULL;
	}

	return ret;
}

void freeLocateTagItemArray(int count, LocateTagItem * array) {
	int i;
	
	for(i = 0; i < count; i++) free(array[i].needle);

	free(array);
}

int newLocateTagItemArrayFromArgArray(char * argArray[],
						int numArgs,
						LocateTagItem ** arrayRet)
{
	int i,j;
	LocateTagItem * item;
	
	if(numArgs == 0) return 0;

	if(numArgs%2 != 0) return -1;
	
	*arrayRet = malloc(sizeof(LocateTagItem)*numArgs/2);

	for(i = 0, item = *arrayRet; i < numArgs/2; i++, item++) {
		if(initLocateTagItem(item, argArray[i*2], argArray[i*2+1]) < 0)
			goto fail;
	}

	return numArgs/2;

fail:
	for(j = 0; j < i; j++) {
		free((*arrayRet)[j].needle);
	}

	free(*arrayRet);
	*arrayRet = NULL;
	return -1;
}

void freeLocateTagItem(LocateTagItem * item) {
	free(item->needle);
	free(item);
}

static int countSongsInDirectory(FILE * fp, Directory * directory, void * data) {
	int * count = (int *)data;

	*count+=directory->songs->numberOfNodes;
	
        return 0;
}

static int printDirectoryInDirectory(FILE * fp, Directory * directory, void * data) {
        if(directory->path) {
		myfprintf(fp,"directory: %s\n", getDirectoryPath(directory));
	}
        return 0;
}

static int printSongInDirectory(FILE * fp, Song * song, void * data) {
	printSongUrl(fp, song);
        return 0;
}

static int strstrSearchTag(Song * song, int type, char * str) {
	int i;
	char * dup;
	int ret = 0;

	if(type == LOCATE_TAG_FILE_TYPE || type == LOCATE_TAG_ANY_TYPE) {
		dup = strDupToUpper(getSongUrl(song));
		if(strstr(dup, str)) ret = 1;
		free(dup);
		if (ret == 1 || type == LOCATE_TAG_FILE_TYPE) {
			return ret;
		}
	}

	if(!song->tag) return 0;

	for(i = 0; i < song->tag->numOfItems && !ret; i++) { 
		if(type != LOCATE_TAG_ANY_TYPE &&
				song->tag->items[i].type != type)
		{
		  	continue;
		}
		
		dup = strDupToUpper(song->tag->items[i].value);
		if(strstr(dup, str)) ret = 1;
		free(dup);
	}

	return ret;
}

static int searchInDirectory(FILE * fp, Song * song, void * data) {
	LocateTagItemArray * array = data;
	int i;

	for(i = 0; i < array->numItems; i++) {
		if(!strstrSearchTag(song, array->items[i].tagType,
				array->items[i].needle)) 
		{
			return 0;
		}
	}

	printSongInfo(fp, song);

	return 0;
}

int searchForSongsIn(FILE * fp, char * name, int numItems, 
		LocateTagItem * items) 
{
	int ret = -1;
	int i;

	char ** originalNeedles = malloc(numItems*sizeof(char *));
	LocateTagItemArray array;

	for(i = 0; i < numItems; i++) {
		originalNeedles[i] = items[i].needle;
		items[i].needle = strDupToUpper(originalNeedles[i]);
	}

	array.numItems = numItems;
	array.items = items;

	ret = traverseAllIn(fp,name,searchInDirectory, NULL, &array);

	for(i = 0; i < numItems; i++) {
		free(items[i].needle);
		items[i].needle = originalNeedles[i];
	}

	free(originalNeedles);

	return ret;
}

static int tagItemFoundAndMatches(Song * song, int type, char * str) {
	int i;

	if(type == LOCATE_TAG_FILE_TYPE) {
		if(0 == strcmp(str, getSongUrl(song))) return 1;
	}

	if(!song->tag) return 0;

	for(i = 0; i < song->tag->numOfItems; i++) {
		if(song->tag->items[i].type != type) continue;
		
		if(0 == strcmp(str, song->tag->items[i].value)) return 1;
	}

	return 0;
}

static int findInDirectory(FILE * fp, Song * song, void * data) {
	LocateTagItemArray * array = data;
	int i;

	for(i = 0; i < array->numItems; i++) {
		if(!tagItemFoundAndMatches(song, array->items[i].tagType,
				array->items[i].needle)) 
		{
			return 0;
		}
	}

	printSongInfo(fp, song);

	return 0;
}

int findSongsIn(FILE * fp, char * name, int numItems, LocateTagItem * items) {
	LocateTagItemArray array;

	array.numItems = numItems;
	array.items = items;
	
	return traverseAllIn(fp, name, findInDirectory, NULL,
			(void *)&array);
}

int printAllIn(FILE * fp, char * name) {
	return traverseAllIn(fp,name,printSongInDirectory,
				printDirectoryInDirectory,NULL);
}

static int directoryAddSongToPlaylist(FILE * fp, Song * song, void * data) {
	return addSongToPlaylist(fp, song, 0);
}

int addAllIn(FILE * fp, char * name) {
	return traverseAllIn(fp,name,directoryAddSongToPlaylist,NULL,NULL);
}

static int directoryPrintSongInfo(FILE * fp, Song * song, void * data) {
	return printSongInfo(fp,song);
}

static int sumSongTime(FILE * fp, Song * song, void * data) {
	unsigned long * time = (unsigned long *)data;

	if(song->tag && song->tag->time>=0) *time+=song->tag->time;

	return 0;
}

int printInfoForAllIn(FILE * fp, char * name) {
        return traverseAllIn(fp,name,directoryPrintSongInfo,printDirectoryInDirectory,NULL);
}

int countSongsIn(FILE * fp, char * name) {
	int count = 0;
	void * ptr = (void *)&count;
	
        traverseAllIn(fp,name,NULL,countSongsInDirectory,ptr);

	return count;
}

unsigned long sumSongTimesIn(FILE * fp, char * name) {
	unsigned long dbPlayTime = 0;
	void * ptr = (void *)&dbPlayTime;
	
        traverseAllIn(fp,name,sumSongTime,NULL,ptr);

	return dbPlayTime;
}

static ListCommandItem * newListCommandItem(int tagType, int numConditionals,
		LocateTagItem * conditionals)
{
	ListCommandItem * item = malloc(sizeof(ListCommandItem));

	item->tagType = tagType;
	item->numConditionals = numConditionals;
	item->conditionals = conditionals;

	return item;
}

static void freeListCommandItem(ListCommandItem * item) {
	free(item);
}

static void visitTag(FILE * fp, Song * song, int tagType) {
	int i;
	MpdTag * tag = song->tag;

	if(tagType == LOCATE_TAG_FILE_TYPE) {
		printSongUrl(fp, song);
		return;
	}

	if(!tag) return;

	for(i = 0; i < tag->numOfItems; i++) {
		if(tag->items[i].type == tagType) {
			visitInTagTracker(tagType, tag->items[i].value);
		}
	}
}

static int listUniqueTagsInDirectory(FILE * fp, Song * song, void * data) {
	ListCommandItem * item = data;
	int i;

	for(i = 0; i < item->numConditionals; i++) {
		if(!tagItemFoundAndMatches(song, item->conditionals[i].tagType,
				item->conditionals[i].needle)) 
		{
			return 0;
		}
	}

	visitTag(fp, song, item->tagType);

	return 0;
}

int listAllUniqueTags(FILE * fp, int type, int numConditionals, 
		LocateTagItem * conditionals)
{
	int ret;
	ListCommandItem * item = newListCommandItem(type, numConditionals,
							conditionals);
	
	if(type >= 0 && type <= TAG_NUM_OF_ITEM_TYPES) {
		resetVisitedFlagsInTagTracker(type);
	}

	ret = traverseAllIn(fp, NULL, listUniqueTagsInDirectory, NULL,
			(void *)item);

	if(type >= 0 && type <= TAG_NUM_OF_ITEM_TYPES) {
		printVisitedInTagTracker(fp, type);
	}

	freeListCommandItem(item);

	return ret;
}

static int sumSavedFilenameMemoryInDirectory(FILE * fp, Directory * dir, void * data) {
	int * sum = data;

	if(!dir->path) return 0;

	*sum += (strlen(getDirectoryPath(dir))+1-sizeof(Directory *))*
				dir->songs->numberOfNodes;

	return 0;
}

static int sumSavedFilenameMemoryInSong(FILE * fp, Song * song, void * data) {
	int * sum = data;

	*sum += strlen(song->url)+1;
	
	return 0;
}

void printSavedMemoryFromFilenames() {
	int sum = 0;

	traverseAllIn(stderr, NULL, sumSavedFilenameMemoryInSong,
		sumSavedFilenameMemoryInDirectory, (void *)&sum);

	DEBUG("saved memory from filenames: %i\n", sum);
}
