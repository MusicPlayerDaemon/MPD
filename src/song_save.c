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

#include "song_save.h"
#include "song_print.h"
#include "directory.h"
#include "path.h"
#include "utils.h"
#include "tag.h"
#include "log.h"

#define SONG_KEY	"key: "
#define SONG_MTIME	"mtime: "

void writeSongInfoFromList(FILE * fp, SongList * list)
{
	ListNode *tempNode = list->firstNode;

	fprintf(fp, "%s\n", SONG_BEGIN);

	while (tempNode != NULL) {
		fprintf(fp, "%s%s\n", SONG_KEY, tempNode->key);
		fflush(fp);
		printSongInfo(fileno(fp), (Song *) tempNode->data);
		fprintf(fp, "%s%li\n", SONG_MTIME,
			  (long)((Song *) tempNode->data)->mtime);
		tempNode = tempNode->nextNode;
	}

	fprintf(fp, "%s\n", SONG_END);
}

static void insertSongIntoList(SongList * list, ListNode ** nextSongNode,
			       char *key, Song * song)
{
	ListNode *nodeTemp;
	int cmpRet = 0;

	while (*nextSongNode
	       && (cmpRet = strcmp(key, (*nextSongNode)->key)) > 0) {
		nodeTemp = (*nextSongNode)->nextNode;
		deleteNodeFromList(list, *nextSongNode);
		*nextSongNode = nodeTemp;
	}

	if (!(*nextSongNode)) {
		insertInList(list, song->url, (void *)song);
	} else if (cmpRet == 0) {
		Song *tempSong = (Song *) ((*nextSongNode)->data);
		if (tempSong->mtime != song->mtime) {
			tag_free(tempSong->tag);
			tag_end_add(song->tag);
			tempSong->tag = song->tag;
			tempSong->mtime = song->mtime;
			song->tag = NULL;
		}
		freeJustSong(song);
		*nextSongNode = (*nextSongNode)->nextNode;
	} else {
		insertInListBeforeNode(list, *nextSongNode, -1, song->url,
				       (void *)song);
	}
}

static int matchesAnMpdTagItemKey(char *buffer, int *itemType)
{
	int i;

	for (i = 0; i < TAG_NUM_OF_ITEM_TYPES; i++) {
		if (0 == strncmp(mpdTagItemKeys[i], buffer,
				 strlen(mpdTagItemKeys[i]))) {
			*itemType = i;
			return 1;
		}
	}

	return 0;
}

void readSongInfoIntoList(FILE * fp, SongList * list, Directory * parentDir)
{
	char buffer[MPD_PATH_MAX + 1024];
	int bufferSize = MPD_PATH_MAX + 1024;
	Song *song = NULL;
	ListNode *nextSongNode = list->firstNode;
	ListNode *nodeTemp;
	int itemType;

	while (myFgets(buffer, bufferSize, fp) && 0 != strcmp(SONG_END, buffer)) {
		if (0 == strncmp(SONG_KEY, buffer, strlen(SONG_KEY))) {
			if (song) {
				insertSongIntoList(list, &nextSongNode,
						   song->url, song);
				if (song->tag != NULL)
					tag_end_add(song->tag);
			}

			song = newNullSong();
			song->url = xstrdup(buffer + strlen(SONG_KEY));
			song->type = SONG_TYPE_FILE;
			song->parentDir = parentDir;
		} else if (*buffer == 0) {
			/* ignore empty lines (starting with '\0') */
		} else if (song == NULL) {
			FATAL("Problems reading song info\n");
		} else if (0 == strncmp(SONG_FILE, buffer, strlen(SONG_FILE))) {
			/* we don't need this info anymore
			   song->url = xstrdup(&(buffer[strlen(SONG_FILE)]));
			 */
		} else if (matchesAnMpdTagItemKey(buffer, &itemType)) {
			if (!song->tag) {
				song->tag = tag_new();
				tag_begin_add(song->tag);
			}

			tag_add_item(song->tag, itemType,
				     &(buffer
				       [strlen(mpdTagItemKeys[itemType]) +
					2]));
		} else if (0 == strncmp(SONG_TIME, buffer, strlen(SONG_TIME))) {
			if (!song->tag) {
				song->tag = tag_new();
				tag_begin_add(song->tag);
			}

			song->tag->time = atoi(&(buffer[strlen(SONG_TIME)]));
		} else if (0 == strncmp(SONG_MTIME, buffer, strlen(SONG_MTIME))) {
			song->mtime = atoi(&(buffer[strlen(SONG_MTIME)]));
		}
		else
			FATAL("songinfo: unknown line in db: %s\n", buffer);
	}

	if (song) {
		insertSongIntoList(list, &nextSongNode, song->url, song);
		if (song->tag != NULL)
			tag_end_add(song->tag);
	}

	while (nextSongNode) {
		nodeTemp = nextSongNode->nextNode;
		deleteNodeFromList(list, nextSongNode);
		nextSongNode = nodeTemp;
	}
}
