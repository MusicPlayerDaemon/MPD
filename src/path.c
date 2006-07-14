/* the Music Player Daemon (MPD)
 * (c)2003-2006 by Warren Dukes (warren.dukes@gmail.com)
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

#include "path.h"
#include "log.h"
#include "charConv.h"
#include "conf.h"
#include "utf8.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>

#ifdef HAVE_LOCALE
#ifdef HAVE_LANGINFO_CODESET
#include <locale.h>
#include <langinfo.h>
#endif
#endif

char * musicDir;
char * playlistDir;

char * fsCharset = NULL;

static char * pathConvCharset(char * to, char * from, char * str) {
	if(setCharSetConversion(to,from)==0) {
		return convStrDup(str);
	}

	return NULL;
}

char * fsCharsetToUtf8(char * str) {
	char * ret = pathConvCharset("UTF-8",fsCharset,str);

	if(ret && !validUtf8String(ret)) {
		free(ret);
		ret = NULL;
	}

	return ret;
}

char * utf8ToFsCharset(char * str) {
	char * ret = pathConvCharset(fsCharset,"UTF-8",str);

	if(!ret) ret = strdup(str);

	return ret;
}

void setFsCharset(char * charset) {
	int error = 0;

	if(fsCharset) free(fsCharset);

	fsCharset = strdup(charset);

	DEBUG("setFsCharset: fs charset is: %s\n",fsCharset);
	
	if(setCharSetConversion("UTF-8",fsCharset)!=0) {
		WARNING("fs charset conversion problem: "
			"not able to convert from \"%s\" to \"%s\"\n",
			fsCharset,"UTF-8");
		error = 1;
	}
	if(setCharSetConversion(fsCharset,"UTF-8")!=0) {
		WARNING("fs charset conversion problem: "
			"not able to convert from \"%s\" to \"%s\"\n",
			"UTF-8",fsCharset);
		error = 1;
	}
	
	if(error) {
		free(fsCharset);
		WARNING("setting fs charset to ISO-8859-1!\n");
		fsCharset = strdup("ISO-8859-1");
	}
}

char * getFsCharset() {
	return fsCharset;
}

static char * appendSlash(char ** path) {
	char * temp = *path;
	int len = strlen(temp);

	if(temp[len-1] != '/') {
		temp = malloc(len+2);
		memset(temp, 0, len+2);
		memcpy(temp, *path, len);
		temp[len] = '/';
		free(*path);
		*path = temp;
	}

	return temp;
}

void initPaths() {
	ConfigParam * musicParam = parseConfigFilePath(CONF_MUSIC_DIR, 1);
	ConfigParam * playlistParam = parseConfigFilePath(CONF_PLAYLIST_DIR, 1);
        ConfigParam * fsCharsetParam = getConfigParam(CONF_FS_CHARSET);

	char * charset = NULL;
	char * originalLocale;
	DIR * dir;

	musicDir = appendSlash(&(musicParam->value));
	playlistDir = appendSlash(&(playlistParam->value));

        if((dir = opendir(playlistDir)) == NULL) {
                ERROR("cannot open %s \"%s\" (config line %i): %s\n", 
				CONF_PLAYLIST_DIR, playlistParam->value, 
				playlistParam->line, strerror(errno));
                exit(EXIT_FAILURE);
        }
	closedir(dir);

        if((dir = opendir(musicDir)) == NULL) {
                ERROR("cannot open %s \"%s\" (config line %i): %s\n", 
				CONF_MUSIC_DIR, musicParam->value, 
				musicParam->line, strerror(errno));
                exit(EXIT_FAILURE);
        }
	closedir(dir);

	if(fsCharsetParam) {
		charset = strdup(fsCharsetParam->value);
	}
#ifdef HAVE_LOCALE
#ifdef HAVE_LANGINFO_CODESET
	else if((originalLocale = setlocale(LC_CTYPE,NULL))) {
		char * temp;
		char * currentLocale;
		originalLocale = strdup(originalLocale);

		if(!(currentLocale = setlocale(LC_CTYPE,""))) {
			WARNING("problems setting current locale with "
					"setlocale()\n");
		}
		else {
			if(strcmp(currentLocale,"C")==0 ||
					strcmp(currentLocale,"POSIX")==0) 
			{
				WARNING("current locale is \"%s\"\n",
						currentLocale);
			}
			else if((temp = nl_langinfo(CODESET))) {
				charset = strdup(temp);
			}
			else WARNING("problems getting charset for locale\n");
			if(!setlocale(LC_CTYPE,originalLocale)) {
				WARNING("problems resetting locale with setlocale()\n");
			}
		}

		free(originalLocale);
	}
	else WARNING("problems getting locale with setlocale()\n");
#endif
#endif

	if(charset) {
		setFsCharset(charset);
		free(charset);
	}
	else {
		WARNING("setting filesystem charset to ISO-8859-1\n");
		setFsCharset("ISO-8859-1");
	}
}

void finishPaths() {
	free(fsCharset);
	fsCharset = NULL;
}

char * rmp2amp(char * relativePath) {
	static char absolutePath[MAXPATHLEN+1];

	memset(absolutePath,0,MAXPATHLEN+1);

	strncpy(absolutePath,musicDir,MAXPATHLEN);
	strncat(absolutePath,relativePath,MAXPATHLEN-strlen(musicDir));

	return absolutePath;
}

char * rpp2app(char * relativePath) {
	static char absolutePath[MAXPATHLEN+1];

	memset(absolutePath,0,MAXPATHLEN+1);

	strncpy(absolutePath,playlistDir,MAXPATHLEN);
	strncat(absolutePath,relativePath,MAXPATHLEN-strlen(musicDir));

	return absolutePath;
}

char * parentPath(char * path) {
	static char parentPath[MAXPATHLEN+1];
	char * c;

	memset(parentPath,0,MAXPATHLEN+1);
	strncpy(parentPath,path,MAXPATHLEN);
	
	c = strrchr(parentPath,'/');
	if (c == NULL)
		parentPath[0] = '\0';
	else {
		while ((parentPath <= c) && *(--c) == '/') /* nothing */;
		c[1] = '\0';
 	}

	return parentPath;
}

char * sanitizePathDup(char * path) {
	int len = strlen(path)+1;
	char * ret = malloc(len);
	char * cp = ret;

	memset(ret,0,len);

	len = 0;

	/* illeminate more than one '/' in a row, like "///" */
	while(*path) {
		while(*path=='/') path++;
		if(*path=='.') {
			/* we dont want to have hidden directoires, or '.' or
			   ".." in our path */
			free(ret);
			return NULL;
		}
		while(*path && *path!='/') {
			*(cp++) = *(path++);
			len++;
		}
		if(*path=='/') {
			*(cp++) = *(path++);
			len++;
		}
	}

	if(len && ret[len-1]=='/') {
		len--;
		ret[len] = '\0';
	}

	DEBUG("sanitized: %s\n", ret);

	return realloc(ret,len+1);
}

