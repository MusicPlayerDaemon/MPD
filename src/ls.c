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
#include "playlist.h"
#include "path.h"
#include "myfprintf.h"
#include "log.h"
#include "utf8.h"

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

static char * remoteUrlPrefixes[] = 	
{
        "http://",
	NULL
};

int printRemoteUrlHandlers(FILE * fp) {
        char ** prefixes = remoteUrlPrefixes;

        while (*prefixes) {
                myfprintf(fp,"handler: %s\n", *prefixes);
                prefixes++;
        }

        return 0;
}

int isValidRemoteUtf8Url(char * utf8url) {
        int ret = 0;
        char * temp;

        switch(isRemoteUrl(utf8url)) {
        case 1:
                ret = 1;
                temp = utf8url;
                while(*temp) {
                        if((*temp >= 'a' && *temp <= 'z') || 
                                        (*temp >= 'A' && *temp <= 'Z') ||
                                        (*temp >= '0' && *temp <= '9') ||
                                        *temp == '$' ||
                                        *temp == '-' ||
                                        *temp == '.' ||
                                        *temp == '+' ||
                                        *temp == '!' ||
                                        *temp == '*' ||
                                        *temp == '\'' ||
                                        *temp == '(' ||
                                        *temp == ')' ||
                                        *temp == ',' ||
                                        *temp == '%' ||
                                        *temp == '/' ||
                                        *temp == ':' ||
                                        *temp == '?' ||
                                        *temp == ';' ||
                                        *temp == '&' ||
                                        *temp == '=') 
                        {
                        }
                        else {
                                ret = 1;
                                break;
                        }
                        temp++;
                }
                break;
        }

        return ret;
}

int isRemoteUrl(char * url) {
        int count = 0;
	char ** urlPrefixes = remoteUrlPrefixes;

	while(*urlPrefixes) {
                count++;
		if(strncmp(*urlPrefixes,url,strlen(*urlPrefixes)) == 0) {
                        return count;
		}
		urlPrefixes++;
	}

	return 0;
}

int lsPlaylists(FILE * fp, char * utf8path) {
	DIR * dir;
	struct stat st;
	struct dirent * ent;
	char * dup;
	char * utf8;
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
	/* this is safe, notice actlen > MAXPATHLEN-1 above */
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
					if(list==NULL) list = makeList(NULL, 1);
					dup = strdup(ent->d_name);
					dup[suff] = '\0';
					if((utf8 = fsCharsetToUtf8(dup))) {
						insertInList(list,utf8,NULL);
					}
					free(dup);
				}
			}
		}
	}
	
	closedir(dir);
	free(path);

	if(list) {
		int i;
		sortList(list);

		dup = malloc(strlen(utf8path)+2);
		strcpy(dup,utf8path);
		for(i = strlen(dup)-1; i >= 0 && dup[i]=='/'; i--) {
			dup[i] = '\0';
		}
		if(strlen(dup)) strcat(dup,"/");

		node = list->firstNode;
		while(node!=NULL) {
                        if(!strchr(node->key, '\n')) {
			        myfprintf(fp,"playlist: %s%s\n",dup,node->key);
                        }
			node = node->nextNode;
		}

		freeList(list);
		free(dup);
	}

	return 0;
}

int myStat(char * utf8file, struct stat * st) {
	char * file = utf8ToFsCharset(utf8file);
	char * actualFile = file;

	if(actualFile[0]!='/') actualFile = rmp2amp(file);

	return stat(actualFile,st);
}

int isFile(char * utf8file, time_t * mtime) {
	struct stat st;

	if(myStat(utf8file,&st)==0) {
		if(S_ISREG(st.st_mode)) {
			if(mtime) *mtime = st.st_mtime;
			return 1;
		}
		else return 0;
	}

	return 0;
}

/* suffixes should be ascii only characters */
char * getSuffix(char * utf8file) {
        char * ret = NULL;
        
        while(*utf8file) {
                if(*utf8file == '.') ret = utf8file+1;
                utf8file++;
        }

	return ret;
}

int hasSuffix(char * utf8file, char * suffix) {
        char * s = getSuffix(utf8file);
        if(s && 0==strcmp(s,suffix)) return 1;
        return 0;
}

int isPlaylist(char * utf8file) {
	if(isFile(utf8file,NULL)) {
		return hasSuffix(utf8file,PLAYLIST_FILE_SUFFIX);
	}
	return 0;
}

int isDir(char * utf8name) {
	struct stat st;

	if(myStat(utf8name,&st)==0) {
		if(S_ISDIR(st.st_mode)) {
			return 1;
		}
	}

	return 0;
}

InputPlugin * hasMusicSuffix(char * utf8file) {
        InputPlugin * ret = NULL;
        
        char * s = getSuffix(utf8file);
        if(s) ret = getInputPluginFromSuffix(s);

	return ret;
}

InputPlugin * isMusic(char * utf8file, time_t * mtime) {
	if(isFile(utf8file,mtime)) {
		return hasMusicSuffix(utf8file);
	}

	return NULL;
}
