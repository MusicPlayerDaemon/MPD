#include "song.h"

#define LOCATE_TAG_FILE_TYPE	TAG_NUM_OF_ITEM_TYPES+10
#define LOCATE_TAG_ANY_TYPE     TAG_NUM_OF_ITEM_TYPES+20

/* struct used for search, find, list queries */
typedef struct _LocateTagItem {
	mpd_sint8 tagType;
	/* what we are looking for */
	char *needle;
} LocateTagItem;

int getLocateTagItemType(char *str);

/* returns NULL if not a known type */
LocateTagItem *newLocateTagItem(char *typeString, char *needle);

/* return number of items or -1 on error */
int newLocateTagItemArrayFromArgArray(char *argArray[], int numArgs,
				      LocateTagItem ** arrayRet);

void freeLocateTagItemArray(int count, LocateTagItem * array);

void freeLocateTagItem(LocateTagItem * item);

int strstrSearchTags(Song * song, int numItems, LocateTagItem * items);

int tagItemsFoundAndMatches(Song * song, int numItems, LocateTagItem * items);
