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
#include "tables.h"
#include "utils.h"
#include "tag.h"
#include "log.h"
#include "mp3_decode.h"
#include "audiofile_decode.h"
#include "ogg_decode.h"
#include "flac_decode.h"
#include "path.h"

#define SONG_KEY	"key: "
#define SONG_FILE	"file: "
#define SONG_ARTIST	"Artist: "
#define SONG_ALBUM	"Album: "
#define SONG_TRACK	"Track: "
#define SONG_TITLE	"Title: "
#define SONG_TIME	"Time: "
#define SONG_MTIME	"mtime: "

#include <stdlib.h>
#include <string.h>

Song * newSong(char * utf8file) {
	Song * song = malloc(sizeof(Song));
	song->time = -1;

	song->utf8file = strdup(utf8file);

	if(0);
#ifdef HAVE_OGG
	else if((song->mtime = isOgg(utf8file))) {
		song->time = getOggTotalTime(
				rmp2amp(utf8ToFsCharset(utf8file)));
		if(song->time>=0) song->tag = oggTagDup(utf8file);
	}
#endif
#ifdef HAVE_FLAC
	else if((song->mtime = isFlac(utf8file))) {
		song->time = getFlacTotalTime(
				rmp2amp(utf8ToFsCharset(utf8file)));
		if(song->time>=0) song->tag = flacTagDup(utf8file);
	}
#endif
#ifdef HAVE_MAD
	else if((song->mtime = isMp3(utf8file))) {
		song->time = getMp3TotalTime(
				rmp2amp(utf8ToFsCharset(utf8file)));
		if(song->time>=0) song->tag = mp3TagDup(utf8file);
	}
#endif
#ifdef HAVE_AUDIOFILE
	else if((song->mtime = isWave(utf8file))) {
		song->time = getAudiofileTotalTime(
				rmp2amp(utf8ToFsCharset(utf8file)));
		if(song->time>=0) song->tag = audiofileTagDup(utf8file);
	}
#endif

	if(song->time<0) {
		freeSong(song);
		song = NULL;
	}

	return song;
}

void freeSong(Song * song) {
	free(song->utf8file);
	if(song->tag) freeMpdTag(song->tag);
	free(song);
}

SongList * newSongList() {
	return makeList((ListFreeDataFunc *)freeSong);
}

Song * addSongToList(SongList * list, char * key, char * utf8file) {
	Song * song = NULL;
	
	if(isMusic(utf8file)) {
		song = newSong(utf8file);
	}

	if(song==NULL) return NULL;
	
	insertInList(list,key,(void *)song);

	return song;
}

void freeSongList(SongList * list) {
	freeList(list);
}

int printSongInfo(FILE * fp, Song * song) {
	myfprintf(fp,"%s%s\n",SONG_FILE,song->utf8file);

	if(song->time>=0) myfprintf(fp,"%s%i\n",SONG_TIME,song->time);

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

void readSongInfoIntoList(FILE * fp, SongList * list) {
	char buffer[MAXPATHLEN+1024];
	int bufferSize = MAXPATHLEN+1024;
	Song * song = NULL;
	char * key = NULL;

	while(myFgets(buffer,bufferSize,fp) && 0!=strcmp(SONG_END,buffer)) {
		if(0==strncmp(SONG_KEY,buffer,strlen(SONG_KEY))) {
			if(song) {
				insertInList(list,key,(void *)song);
				addSongToTables(song);
				free(key);
			}
			key = strdup(&(buffer[strlen(SONG_KEY)]));
			song = malloc(sizeof(Song));
			song->tag = NULL;
			song->utf8file = NULL;
		}
		else if(0==strncmp(SONG_FILE,buffer,strlen(SONG_FILE))) {
			if(!song || song->utf8file) {
				ERROR("Problems reading song info\n");
				exit(-1);
			}
			song->utf8file = strdup(&(buffer[strlen(SONG_FILE)]));
		}
		else if(0==strncmp(SONG_ARTIST,buffer,strlen(SONG_ARTIST))) {
			if(!song->tag) song->tag = newMpdTag();
			song->tag->artist = strdup(&(buffer[strlen(SONG_ARTIST)]));
		}
		else if(0==strncmp(SONG_ALBUM,buffer,strlen(SONG_ALBUM))) {
			if(!song->tag) song->tag = newMpdTag();
			song->tag->album = strdup(&(buffer[strlen(SONG_ALBUM)]));
		}
		else if(0==strncmp(SONG_TRACK,buffer,strlen(SONG_TRACK))) {
			if(!song->tag) song->tag = newMpdTag();
			song->tag->track = strdup(&(buffer[strlen(SONG_TRACK)]));
		}
		else if(0==strncmp(SONG_TITLE,buffer,strlen(SONG_TITLE))) {
			if(!song->tag) song->tag = newMpdTag();
			song->tag->title = strdup(&(buffer[strlen(SONG_TITLE)]));
		}
		else if(0==strncmp(SONG_TIME,buffer,strlen(SONG_TIME))) {
			song->time = atoi(&(buffer[strlen(SONG_TIME)]));
		}
		else if(0==strncmp(SONG_MTIME,buffer,strlen(SONG_MTIME))) {
			song->mtime = atoi(&(buffer[strlen(SONG_TITLE)]));
		}
		else {
			ERROR("songinfo: unknown line in db: %s\n",buffer);
			exit(-1);
		}
	}
	
	if(song) {
		insertInList(list,key,(void *)song);
		addSongToTables(song);
		free(key);
	}
}

int updateSongInfo(Song * song) {
	if(song->tag) freeMpdTag(song->tag);
#ifdef HAVE_MAD
	if((song->mtime = isMp3(song->utf8file))) {
		song->tag = mp3TagDup(song->utf8file);
		return 0;
	}
#endif
#ifdef HAVE_OGG
	if((song->mtime = isOgg(song->utf8file))) {
		song->tag = oggTagDup(song->utf8file);
		return 0;
	}
#endif
#ifdef HAVE_FLAC
	if((song->mtime = isFlac(song->utf8file))) {
		song->tag = flacTagDup(song->utf8file);
		return 0;
	}
#endif
#ifdef HAVE_AUDIOFILE
	if((song->mtime = isWave(song->utf8file))) {
		song->tag = audiofileTagDup(song->utf8file);
		return 0;
	}
#endif
	return -1;
}

Song * songDup(Song * song) {
	Song * ret = malloc(sizeof(Song));

	ret->utf8file = strdup(song->utf8file);
	ret->mtime = song->mtime;
	ret->tag = mpdTagDup(song->tag);

	return ret;
}
