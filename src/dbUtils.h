#ifndef DB_UTILS_H
#define DB_UTILS_H

#include <stdio.h>

#include "tag.h"

/* struct used for search, find, list queries */
typedef struct _LocateTagItem {
	mpd_sint8 tagType;
	/* what we are looking for */
	char * needle;
} LocateTagItem;

int getLocateTagItemType(char * str);

/* returns NULL if not a known type */
LocateTagItem * newLocateTagItem(char * typeString, char * needle);

/* return number of items or -1 on error */
int newLocateTagItemArrayFromArgArray(char * argArray[], int numArgs,
					LocateTagItem ** arrayRet);
						

void freeLocateTagItemArray(int count, LocateTagItem * array);

void freeLocateTagItem(LocateTagItem * item);

int printAllIn(FILE * fp, char * name);

int addAllIn(FILE * fp, char * name);

int printInfoForAllIn(FILE * fp, char * name);

int searchForSongsIn(FILE * fp, char * name, int numItems,
				LocateTagItem * items);

int findSongsIn(FILE * fp, char * name, int numItems, 
				LocateTagItem * items);

int countSongsIn(FILE * fp, char * name);

unsigned long sumSongTimesIn(FILE * fp, char * name);

int listAllUniqueTags(FILE * fp, int type, int numConditiionals, 
		LocateTagItem * conditionals);

void printSavedMemoryFromFilenames();

void printSavedMemoryFromDirectoryNames();

#endif
