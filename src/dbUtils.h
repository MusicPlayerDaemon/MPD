#ifndef DB_UTILS_H
#define DB_UTILS_H

#include <stdio.h>

#include "tag.h"

#define LOCATE_TAG_FILE_TYPE	TAG_NUM_OF_ITEM_TYPES+10
#define LOCATE_TAG_FILE_KEY	"filename"

/* struct used for search, find, list queries */
typedef struct _LocateTagItem {
	mpd_sint8 tagType;
	/* what we are looking for */
	char * needle;
} LocateTagItem;

/* returns NULL if not a known type */
LocateTagItem * newLocateTagItem(char * typeString, char * needle);

int getLocateTagItemType(char * str);

void freeLocateTagItem(LocateTagItem * item);

int printAllIn(FILE * fp, char * name);

int addAllIn(FILE * fp, char * name);

int printInfoForAllIn(FILE * fp, char * name);

int searchForSongsIn(FILE * fp, char * name, LocateTagItem * item);

int findSongsIn(FILE * fp, char * name, LocateTagItem * item);

int countSongsIn(FILE * fp, char * name);

unsigned long sumSongTimesIn(FILE * fp, char * name);

int listAllUniqueTags(FILE * fp, int type, int numConditiionals, 
		LocateTagItem * conditionals);

#endif
