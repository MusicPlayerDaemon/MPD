#include "dbUtils.h"

#include "directory.h"
#include "myfprintf.h"
#include "utils.h"
#include "playlist.h"
#include "tag.h"
#include "tagTracker.h"

typedef struct ListCommandItem {
	mpd_sint8 tagType;
	int numConditionals;
	LocateTagItem * conditionals;
} ListCommandItem;

int getLocateTagItemType(char * str) {
	int i;

	if(0 == strcasecmp(str, LOCATE_TAG_FILE_KEY)) {
		return LOCATE_TAG_FILE_TYPE;
	}

	for(i = 0; i < TAG_NUM_OF_ITEM_TYPES; i++) {
		if(0 == strcasecmp(str, mpdTagItemKeys[i])) return i;
	}

	return -1;
}

LocateTagItem * newLocateTagItem(char * typeStr, char * needle) {
	LocateTagItem * ret = malloc(sizeof(LocateTagItem));

	ret->tagType = getLocateTagItemType(typeStr);

	if(ret->tagType < 0) {
		free(ret);
		return NULL;
	}

	ret->needle = strdup(needle);

	return ret;
}

void freeLocateTagItem(LocateTagItem * item) {
	free(item->needle);
	free(item);
}

int countSongsInDirectory(FILE * fp, Directory * directory, void * data) {
	int * count = (int *)data;

	*count+=directory->songs->numberOfNodes;
	
        return 0;
}

int printDirectoryInDirectory(FILE * fp, Directory * directory, void * data) {
        if(directory->utf8name) {
		myfprintf(fp,"directory: %s\n",directory->utf8name);
	}
        return 0;
}

int printSongInDirectory(FILE * fp, Song * song, void * data) {
        myfprintf(fp,"file: %s\n",song->utf8url);
        return 0;
}

static inline int strstrSearchTag(Song * song, int type, char * str) {
	int i;
	char * dup;
	int ret = 0;

	if(type == LOCATE_TAG_FILE_TYPE) {
		dup = strDupToUpper(song->utf8url);
		if(strstr(dup, str)) ret = 1;
		free(dup);
		return ret;
	}

	if(!song->tag) return 0;

	for(i = 0; i < song->tag->numOfItems && !ret; i++) { 
		if(song->tag->items[i].type != type) continue;
		
		dup = strDupToUpper(song->tag->items[i].value);
		if(strstr(dup, str)) ret = 1;
		free(dup);
	}

	return ret;
}

int searchInDirectory(FILE * fp, Song * song, void * item) {
	if(strstrSearchTag(song, ((LocateTagItem *)item)->tagType,
			((LocateTagItem *)item)->needle)) {
		printSongInfo(fp, song);
	}
	return 0;
}

int searchForSongsIn(FILE * fp, char * name, LocateTagItem * item) {
	char * originalNeedle = item->needle;
	int ret = -1;

	item->needle = strDupToUpper(originalNeedle);

	ret = traverseAllIn(fp,name,searchInDirectory, NULL,
			(void *)item);

	free(item->needle);
	item->needle = originalNeedle;

	return ret;
}

static inline int tagItemFoundAndMatches(Song * song, int type, char * str) {
	int i;

	if(type == LOCATE_TAG_FILE_TYPE) {
		if(0 == strcmp(str, song->utf8url)) return 1;
	}

	if(!song->tag) return 0;

	for(i = 0; i < song->tag->numOfItems; i++) {
		if(song->tag->items[i].type != type) continue;
		
		if(0 == strcmp(str, song->tag->items[i].value)) return 1;
	}

	return 0;
}

int findInDirectory(FILE * fp, Song * song, void * item) {
	if(tagItemFoundAndMatches(song, ((LocateTagItem *)item)->tagType,
			((LocateTagItem *)item)->needle)) {
		printSongInfo(fp, song);
	}
	return 0;
}

int findSongsIn(FILE * fp, char * name, LocateTagItem * item) {
	return traverseAllIn(fp, name, findInDirectory, NULL,
			(void *)item);
}

int printAllIn(FILE * fp, char * name) {
	return traverseAllIn(fp,name,printSongInDirectory,
				printDirectoryInDirectory,NULL);
}

int directoryAddSongToPlaylist(FILE * fp, Song * song, void * data) {
	return addSongToPlaylist(fp, song, 0);
}

int addAllIn(FILE * fp, char * name) {
	return traverseAllIn(fp,name,directoryAddSongToPlaylist,NULL,NULL);
}

int directoryPrintSongInfo(FILE * fp, Song * song, void * data) {
	return printSongInfo(fp,song);
}

int sumSongTime(FILE * fp, Song * song, void * data) {
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

ListCommandItem * newListCommandItem(int tagType, int numConditionals,
		LocateTagItem * conditionals)
{
	ListCommandItem * item = malloc(sizeof(ListCommandItem));

	item->tagType = tagType;
	item->numConditionals = numConditionals;
	item->conditionals = conditionals;

	return item;
}

void freeListCommandItem(ListCommandItem * item) {
	free(item);
}

void printUnvisitedTags(FILE * fp, Song * song, int tagType) {
	int i;
	MpdTag * tag = song->tag;

	if(tagType == LOCATE_TAG_FILE_TYPE) {
		myfprintf(fp, "file: %s\n", song->utf8url);
		return;
	}

	if(!tag) return;

	for(i = 0; i < tag->numOfItems; i++) {
		if(tag->items[i].type == tagType && 
			!wasVisitedInTagTracker(tagType, tag->items[i].value))
		{
			myfprintf(fp, "%s: %s\n", mpdTagItemKeys[tagType],
					tag->items[i].value);
		}
	}
}

int listUniqueTagsInDirectory(FILE * fp, Song * song, void * data) {
	ListCommandItem * item = data;
	int i;

	for(i = 0; i < item->numConditionals; i++) {
		if(!tagItemFoundAndMatches(song, item->conditionals[i].tagType,
				item->conditionals[i].needle)) 
		{
			return 0;
		}
	}

	printUnvisitedTags(fp, song, item->tagType);

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

	freeListCommandItem(item);

	return ret;
}
