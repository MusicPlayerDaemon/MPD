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


#include "tables.h"
#include "list.h"
#include "command.h"
#include "utils.h"
#include "myfprintf.h"

#include <string.h>

#define TABLES_ARTIST	"artist"
#define TABLES_ALBUM	"album"

List * albumTable;
List * artistTable;

typedef struct _ArtistData {
	int songs;
	List * albums;
} ArtistData;

ArtistData * newArtistData() {
	ArtistData * ad = malloc(sizeof(ArtistData));

	ad->songs = 0;
	ad->albums = makeList(free);

	return ad;
}

void freeArtistData(ArtistData * ad) {
	freeList(ad->albums);
}

void initTables() {
	albumTable = makeList(free);
	artistTable = makeList((ListFreeDataFunc *)freeArtistData);
}

void closeTables() {
	freeList(albumTable);
	freeList(artistTable);
}

void addSongToSomeAlbumTable(List * table, Song * song) {
	void * songs;
	if(!song->tag) return;
	if(!song->tag->album || !strlen(song->tag->album)) return;
	if(!findInList(table,song->tag->album,&songs)) {
		songs = malloc(sizeof(int));
		*((int *)songs) = 0;
		insertInList(table,song->tag->album,songs);
	}
	(*((int *)songs))++;
}

void addSongToAlbumTable(Song * song) {
	addSongToSomeAlbumTable(albumTable,song);
}

void addSongToArtistTable(Song * song) {
	void * artist;
	if(!song->tag) return;
	if(!song->tag->artist || !strlen(song->tag->artist)) return;
	if(!findInList(artistTable,song->tag->artist,&artist)) {
		artist = newArtistData();
		insertInList(artistTable,song->tag->artist,artist);
	}
	((ArtistData *)artist)->songs++;
	addSongToSomeAlbumTable(((ArtistData *)artist)->albums,song);
}

void addSongToTables(Song * song) {
	addSongToAlbumTable(song);
	addSongToArtistTable(song);
}

void removeSongFromSomeAlbumTable(List * table, Song * song) {
	void * songs;

	if(!song->tag) return;
	if(!song->tag->album || !strlen(song->tag->album)) return;	
	if(findInList(table,song->tag->album,&songs)) {
		(*((int *)songs))--;
		if(*((int *)songs)<=0) {
			deleteFromList(table,song->tag->album);
		}
	}
}

void removeSongFromAlbumTable(Song * song) {
	removeSongFromSomeAlbumTable(albumTable,song);
}

void removeSongFromArtistTable(Song * song) {
	void * artist;

	if(!song->tag) return;
	if(!song->tag->artist || !strlen(song->tag->artist)) return;	
	if(findInList(artistTable,song->tag->artist,&artist)) {
		removeSongFromSomeAlbumTable(((ArtistData *)artist)->albums,
			song);
		((ArtistData*)artist)->songs--;
		if(((ArtistData *)artist)->songs<=0) {
			deleteFromList(artistTable,song->tag->artist);
		}
	}
}

void removeASongFromTables(Song * song) {
	removeSongFromAlbumTable(song);
	removeSongFromArtistTable(song);
}

unsigned long numberOfSongs() {
	return 0;
}

unsigned long numberOfArtists() {
	return artistTable->numberOfNodes;
}

unsigned long numberOfAlbums() {
	return albumTable->numberOfNodes;
}

int printAllArtists(FILE * fp) {
	ListNode * node = artistTable->firstNode;

	while(node) {
		myfprintf(fp,"Artist: %s\n",node->key);
		node = node->nextNode;
	}

	return 0;
}

int printAllAlbums(FILE * fp, char * artist) {
	if(artist==NULL) {
		ListNode * node = albumTable->firstNode;

		while(node) {
			myfprintf(fp,"Album: %s\n",node->key);
			node = node->nextNode;
		}
	}
	else {
		void * ad;

		if(findInList(artistTable,artist,&ad)) {
			ListNode * node = ((ArtistData *)ad)->albums->firstNode;
			while(node) {
				myfprintf(fp,"Album: %s\n",node->key);
				node = node->nextNode;
			}
		}
		else {
			commandError(fp, "artist \"%s\" not found", artist);
			return -1;
		}
	}

	return 0;
}

int printAllKeysOfTable(FILE * fp, char * table, char * arg1) {
	if(strcmp(table,TABLES_ARTIST)==0) {
		if(arg1!=NULL) {
			commandError(fp, "%s table takes no args", table);
			return -1;
		}
		return printAllArtists(fp);
	}
	else if(strcmp(table,TABLES_ALBUM)==0) {
		return printAllAlbums(fp,arg1);
	}
	else {
		commandError(fp, "table \"%s\" does not exist", table);
		return -1;
	}
}
