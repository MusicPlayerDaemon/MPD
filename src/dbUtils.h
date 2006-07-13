/* the Music Player Daemon (MPD)
 * (c)2003-2006 by Warren Dukes (shank@mercury.chem.pitt.edu)
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

#endif
