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

#include "ls.h"
#include "command.h"
#include "playlist.h"
#include "path.h"
#include "myfprintf.h"
#include "log.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

char * dupAndStripPlaylistSuffix(char * file) {
	size_t size = strlen(file)-strlen(PLAYLIST_FILE_SUFFIX)-1;
	char * ret = malloc(size+1);

	strncpy(ret,file,size);
	ret[size] = '\0';

	return ret;
}

int lsPlaylists(FILE * fp, char * utf8path) {
	DIR * dir;
	struct stat st;
	struct dirent * ent;
	char * dup;
	char s[MAXPATHLEN+1];
	List * list = NULL;
	ListNode * node = NULL;
	char * path = strdup(utf8ToFsCharset(utf8path));
	char * actualPath = rpp2app(path);
	int actlen = strlen(actualPath)+1;
	int maxlen = MAXPATHLEN-actlen;
	int suflen = strlen(PLAYLIST_FILE_SUFFIX)+1;
	int suff;

	if(actlen>MAXPATHLEN-1 || (dir = opendir(actualPath))==NULL) {
		free(path);
		return 0;
	}

	s[MAXPATHLEN] = '\0';
	strcpy(s,actualPath);
	strcat(s,"/");

	while((ent = readdir(dir))) {
		dup = ent->d_name;
		if(dup[0]!='.' && 
				(suff=strlen(dup)-suflen)>0 && 
				dup[suff]=='.' &&
				strcmp(dup+suff+1,PLAYLIST_FILE_SUFFIX)==0) 
		{
			strncpy(s+actlen,ent->d_name,maxlen);
			if(stat(s,&st)==0) {
				if(S_ISREG(st.st_mode)) {
					if(list==NULL) list = makeList(NULL);
					dup = strdup(ent->d_name);
					dup[suff] = '\0';
					insertInList(list,
						fsCharsetToUtf8(dup),NULL);
					free(dup);
				}
			}
		}
	}
	
	closedir(dir);
	free(path);

	if(list) {
		sortList(list);

		dup = malloc(strlen(utf8path)+2);
		strcpy(dup,utf8path);
		while(dup[strlen(dup)-1]=='/') dup[strlen(dup)-1] = '\0';
		if(strlen(dup)) strcat(dup,"/");

		node = list->firstNode;
		while(node!=NULL) {
			myfprintf(fp,"playlist: %s%s\n",dup,node->key);
			node = node->nextNode;
		}

		freeList(list);
		free(dup);
	}

	return 0;
}

int isMusic(char * utf8file, time_t * mtime) {
	int ret = 0;

#ifdef HAVE_OGG
	if((ret = isOgg(utf8file,mtime))) return ret;
#endif
#ifdef HAVE_FLAC
	if((ret = isFlac(utf8file,mtime))) return ret;
#endif
#ifdef HAVE_MAD
	if((ret = isMp3(utf8file,mtime))) return ret;
#endif
#ifdef HAVE_AUDIOFILE
	if((ret = isWave(utf8file,mtime))) return ret;
#endif

	return ret;
}

int isPlaylist(char * utf8file) {
	struct stat st;
	char * file = utf8ToFsCharset(utf8file);
	char * actualFile = file;
	char * temp = NULL;

	if(actualFile[0]!='/') actualFile = rpp2app(file);

	if(stat(actualFile,&st)==0) {
		if(S_ISREG(st.st_mode)) {
			char * dup;
			char * cLast;
			char * cNext;
			int ret = 0;
			dup = strdup(file);
			cNext = cLast = strtok(dup,".");
			while((cNext = strtok(NULL,"."))) cLast = cNext;
			if(cLast && 0==strcmp(cLast,PLAYLIST_FILE_SUFFIX)) {
				ret = 1;
			}
			free(dup);
			if(temp) free(temp);
			return ret;
		}
	}

	if(temp) free(temp);
	return 0;
}

int isWave(char * utf8file, time_t * mtime) {
	struct stat st;
	char * file = utf8ToFsCharset(utf8file);
	char * actualFile = file;

	if(actualFile[0]!='/') actualFile = rmp2amp(file);

	if(stat(actualFile,&st)==0) {
		if(S_ISREG(st.st_mode)) {
			char * dup;
			char * cLast;
			char * cNext;
			int ret = 0;
			dup = strdup(file);
			cNext = cLast = strtok(dup,".");
			while((cNext = strtok(NULL,"."))) cLast = cNext;
			if(cLast && 0==strcasecmp(cLast,"wav")) {
				if(mtime) *mtime = st.st_mtime;
				ret = 1;
			}
			free(dup);
			return ret;
		}
		else return 0;
	}

	return 0;
}

int isFlac(char * utf8file, time_t * mtime) {
	struct stat st;
	char * file = utf8ToFsCharset(utf8file);
	char * actualFile = file;

	if(actualFile[0]!='/') actualFile = rmp2amp(file);

	if(stat(actualFile,&st)==0) {
		if(S_ISREG(st.st_mode)) {
			char * dup;
			char * cLast;
			char * cNext;
			int ret = 0;
			dup = strdup(file);
			cNext = cLast = strtok(dup,".");
			while((cNext = strtok(NULL,"."))) cLast = cNext;
			if(cLast && 0==strcasecmp(cLast,"flac")) {
				if(mtime) *mtime = st.st_mtime;
				ret = 1;
			}
			free(dup);
			return ret;
		}
		else return 0;
	}

	return 0;
}

int isOgg(char * utf8file, time_t * mtime) {
	struct stat st;
	char * file = utf8ToFsCharset(utf8file);
	char * actualFile = file;

	if(actualFile[0]!='/') actualFile = rmp2amp(file);

	if(stat(actualFile,&st)==0) {
		if(S_ISREG(st.st_mode)) {
			char * dup;
			char * cLast;
			char * cNext;
			int ret = 0;
			dup = strdup(file);
			cNext = cLast = strtok(dup,".");
			while((cNext = strtok(NULL,"."))) cLast = cNext;
			if(cLast && 0==strcasecmp(cLast,"ogg")) {
				if(mtime) *mtime = st.st_mtime;
				ret = 1;
			}
			free(dup);
			return ret;
		}
		else return 0;
	}

	return 0;
}

int isMp3(char * utf8file, time_t * mtime) {
	struct stat st;
	char * file = utf8ToFsCharset(utf8file);
	char * actualFile = file;

	if(actualFile[0]!='/') actualFile = rmp2amp(file);

	if(stat(actualFile,&st)==0) {
		if(S_ISREG(st.st_mode)) {
			char * dup;
			char * cLast;
			char * cNext;
			int ret = 0;
			dup = strdup(file);
			cNext = cLast = strtok(dup,".");
			while((cNext = strtok(NULL,"."))) cLast = cNext;
			if(cLast && 0==strcasecmp(cLast,"mp3")) {
				if(mtime) *mtime = st.st_mtime;
				ret = 1;
			}
			free(dup);
			return ret;
		}
		else return 0;
	}

	return 0;
}

int isDir(char * utf8name, time_t * mtime) {
	struct stat st;

	if(stat(rmp2amp(utf8ToFsCharset(utf8name)),&st)==0) {
		if(S_ISDIR(st.st_mode)) {
			if(mtime) *mtime = st.st_mtime;
			return 1;
		}
	}
	else {
		DEBUG("isDir: unable to stat: %s (%s)\n",utf8name,
				rmp2amp(utf8ToFsCharset(utf8name)));
	}

	return 0;
}
