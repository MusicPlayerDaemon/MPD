/* the Music Player Daemon (MPD)
 * (c)2003-2004 by Warren Dukes (shank@mercury.chem.pitt.edu)
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

#include "song.h"
#include "ls.h"
#include "directory.h"
#include "utils.h"
#include "tag.h"
#include "log.h"
#include "path.h"
#include "playlist.h"
#include "inputPlugin.h"

#define SONG_KEY	"key: "
#define SONG_FILE	"file: "
#define SONG_TIME	"Time: "
#define SONG_MTIME	"mtime: "

#include <stdlib.h>
#include <string.h>

Song * newNullSong() {
	Song * song = malloc(sizeof(Song));

	song->tag = NULL;
	song->utf8url = NULL;
	song->type = SONG_TYPE_FILE;
	song->parentDir = NULL;

	return song;
}

Song * newSong(char * url, int type, Directory * parentDir) {
	Song * song = NULL;

        if(strchr(utf8url, '\n')) return NULL;

        song  = newNullSong();

	song->utf8url = strdup(utf8url);
	song->type = type;
	song->parentDir = parentDir;

	assert(type == SONG_TYPE_URL || parentDir);

	if(song->type == SONG_TYPE_FILE) {
                InputPlugin * plugin;
		if((plugin = isMusic(utf8url,&(song->mtime)))) {
		        song->tag = plugin->tagDupFunc(
                                        rmp2amp(utf8ToFsCharset(utf8url)));
                }
		if(!song->tag || song->tag->time<0) {
			freeSong(song);
			song = NULL;
		}
	}

	return song;
}

void freeSong(Song * song) {
	deleteASongFromPlaylist(song);
	free(song->utf8url);
	if(song->tag) freeMpdTag(song->tag);
	free(song);
}

void freeJustSong(Song * song) {
	free(song->utf8url);
	if(song->tag) freeMpdTag(song->tag);
	free(song);
}

SongList * newSongList() {
	return makeList((ListFreeDataFunc *)freeSong);
}

Song * addSongToList(SongList * list, char * url, int songType, 
		Directory * parentDirectory)
{
	Song * song = NULL;

	switch(type) {
	case SONG_TYPE_FILE:
		if(isMusic(utf8url,NULL)) {
			song = newSong(url, type, parentDirectory);
		}
		break;
	case SONG_TYPE_URL:
		song = newSong(utf8url, type, parentDirectory);
		break;
	}

	if(song==NULL) return NULL;
	
	insertInList(list, url, (void *)song);

	return song;
}

void freeSongList(SongList * list) {
	freeList(list);
}

void printSongUrl(FILE * fp, Song * song) {
	if(song->parentDir) {
		myfprintf(fp, "%s%s%s\n", SONG_FILE, song->parentDir->utf8name,
				song->url);
	}
	else {
		myfprintf(fp, "%s%s\n", SONG_FILE, song->url);
	}
}

int printSongInfo(FILE * fp, Song * song) {
	printSongUrl(fp, song);

	if(song->tag) printMpdTag(fp,song->tag);

	return 0;
}

int printSongInfoFromList(FILE * fp, SongList * list) {
	ListNode * tempNode = list->firstNode;

	while(tempNode!=NULL) {
		printSongInfo(fp,(Song *)tempNode->data);
		tempNode = tempNode->nextNode;
	}

	return 0;
}

void writeSongInfoFromList(FILE * fp, SongList * list) {
	ListNode * tempNode = list->firstNode;

	myfprintf(fp,"%s\n",SONG_BEGIN);

	while(tempNode!=NULL) {
		myfprintf(fp,"%s%s\n",SONG_KEY,tempNode->key);
		printSongInfo(fp,(Song *)tempNode->data);
		myfprintf(fp,"%s%li\n",SONG_MTIME,(long)((Song *)tempNode->data)->mtime);
		tempNode = tempNode->nextNode;
	}

	myfprintf(fp,"%s\n",SONG_END);
}

void insertSongIntoList(SongList * list, ListNode ** nextSongNode, char * key,
		Song * song)
{
	ListNode * nodeTemp;
	int cmpRet= 0;

	while(*nextSongNode && (cmpRet = strcmp(key,(*nextSongNode)->key)) > 0) 
	{
		nodeTemp = (*nextSongNode)->nextNode;
		deleteNodeFromList(list,*nextSongNode);
		*nextSongNode = nodeTemp;
	}

	if(!(*nextSongNode)) {
		insertInList(list,key,(void *)song);
	}
	else if(cmpRet == 0) {
		Song * tempSong = (Song *)((*nextSongNode)->data);
		if(tempSong->mtime != song->mtime) {
			freeMpdTag(tempSong->tag);
			tempSong->tag = song->tag;
			tempSong->mtime = song->mtime;
			song->tag = NULL;
		}
		freeJustSong(song);
		*nextSongNode = (*nextSongNode)->nextNode;
	}
	else {
		insertInListBeforeNode(list,*nextSongNode,key,(void *)song);
	}
}

static int matchesAnMpdTagItemKey(char * buffer, int * itemType) {
	int i;

	for(i = 0; i < TAG_NUM_OF_ITEM_TYPES; i++) {
		if( 0 == strncmp(mpdTagItemKeys[i], buffer, 
				strlen(mpdTagItemKeys[i])))
		{
			*itemType = i;
			return 1;
		}
	}

	return 0;
}

void readSongInfoIntoList(FILE * fp, SongList * list) {
	char buffer[MAXPATHLEN+1024];
	int bufferSize = MAXPATHLEN+1024;
	Song * song = NULL;
	char * key = NULL;
	ListNode * nextSongNode = list->firstNode;
	ListNode * nodeTemp;
	int itemType;

	while(myFgets(buffer,bufferSize,fp) && 0!=strcmp(SONG_END,buffer)) {
		if(0==strncmp(SONG_KEY,buffer,strlen(SONG_KEY))) {
			if(song) {
				insertSongIntoList(list,&nextSongNode,key,song);
				song = NULL;
				free(key);
			}

			key = strdup(&(buffer[strlen(SONG_KEY)]));
			song = newNullSong();
			song->type = SONG_TYPE_FILE;
		}
		else if(0==strncmp(SONG_FILE,buffer,strlen(SONG_FILE))) {
			if(!song || song->utf8url) {
				ERROR("Problems reading song info\n");
				exit(EXIT_FAILURE);
			}
			song->utf8url = strdup(&(buffer[strlen(SONG_FILE)]));
		}
		else if(matchesAnMpdTagItemKey(buffer, &itemType)) {
			if(!song->tag) song->tag = newMpdTag();
			addItemToMpdTag(song->tag, itemType,
				&(buffer[strlen(mpdTagItemKeys[itemType])+2]));
		}
		else if(0==strncmp(SONG_TIME,buffer,strlen(SONG_TIME))) {
			if(!song->tag) song->tag = newMpdTag();
			song->tag->time = atoi(&(buffer[strlen(SONG_TIME)]));
		}
		else if(0==strncmp(SONG_MTIME,buffer,strlen(SONG_MTIME))) {
			song->mtime = atoi(&(buffer[strlen(SONG_MTIME)]));
		}
		else {
			ERROR("songinfo: unknown line in db: %s\n",buffer);
			exit(EXIT_FAILURE);
		}
	}
	
	if(song) {
		insertSongIntoList(list,&nextSongNode,key,song);
		song = NULL;
		free(key);
	}

	while(nextSongNode) {
		nodeTemp = nextSongNode->nextNode;
		deleteNodeFromList(list,nextSongNode);
		nextSongNode = nodeTemp;
	}
}

int updateSongInfo(Song * song) {
	char * utf8url = song->utf8url;

	if(song->type == SONG_TYPE_FILE) {
                InputPlugin * plugin;

		if(song->tag) freeMpdTag(song->tag);

		song->tag = NULL;

		if((plugin = isMusic(utf8url,&(song->mtime)))) {
		        song->tag = plugin->tagDupFunc(
                                        rmp2amp(utf8ToFsCharset(utf8url)));
                }
		if(!song->tag || song->tag->time<0) return -1;
	}

	return 0;
}

Song * songDup(Song * song) {
	Song * ret = malloc(sizeof(Song));

	ret->utf8url = strdup(song->utf8url);
	ret->mtime = song->mtime;
	ret->tag = mpdTagDup(song->tag);
	ret->type = song->type;

	return ret;
}
