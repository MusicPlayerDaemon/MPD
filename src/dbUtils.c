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
        if(directory->name) {
		myfprintf(fp,"directory: %s\n", getDirectoryPath(directory));
	}
        return 0;
}

int printSongInDirectory(FILE * fp, Song * song, void * data) {
	printSongUrl(fp, song);
        return 0;
}

static inline int strstrSearchTag(Song * song, int type, char * str) {
	int i;
	char * dup;
	int ret = 0;

	if(type == LOCATE_TAG_FILE_TYPE) {
		dup = strDupToUpper(getSongUrl(song));
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
		if(0 == strcmp(str, getSongUrl(song))) return 1;
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

void visitTag(FILE * fp, Song * song, int tagType) {
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

int sumSavedFilenameMemoryInDirectory(FILE * fp, Directory * dir, void * data) {
	int * sum = data;

	if(!dir->name) return 0;

	*sum += (strlen(getDirectoryPath(dir))+1-sizeof(Directory *))*
				dir->songs->numberOfNodes;

	return 0;
}

int sumSavedFilenameMemoryInSong(FILE * fp, Song * song, void * data) {
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

int sumSavedDirectoryNameMemoryInDirectory(FILE * fp, Directory * dir, void * data) {
	int * sum = data;

	if(!dir->name) return 0;

	*sum += (strlen(getDirectoryPath(dir))+1)*
				dir->subDirectories->numberOfNodes;

	*sum += strlen(dir->name)+1;

	return 0;
}

void printSavedMemoryFromDirectoryNames() {
	int sum = 0;
	
	traverseAllIn(stderr, NULL, NULL, 
			sumSavedDirectoryNameMemoryInDirectory, (void *)&sum);

	DEBUG("saved memory from directory names: %i\n", sum);
}
