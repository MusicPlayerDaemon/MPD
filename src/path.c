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

#include "path.h"
#include "log.h"
#include "charConv.h"
#include "conf.h"

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_LOCALE
#ifdef HAVE_LANGINFO
#include <locale.h>
#include <langinfo.h>
#endif
#endif

char musicDir[MAXPATHLEN+1];
char playlistDir[MAXPATHLEN+1];

char * fsCharset = NULL;

char * pathConvCharset(char * to, char * from, char * str, char * ret) {
	if(ret) {
		free(ret);
		ret = NULL;
	}

	if(setCharSetConversion(to,from)==0) {
		ret = convStrDup(str);
	}

	if(!ret) ret = strdup(str);

	return ret;
}

char * fsCharsetToUtf8(char * str) {
	static char * ret = NULL;

	return ret = pathConvCharset("UTF-8",fsCharset,str,ret);
}

char * utf8ToFsCharset(char * str) {
	static char * ret = NULL;

	return ret = pathConvCharset(fsCharset,"UTF-8",str,ret);
}

void setFsCharset(char * charset) {
	if(fsCharset) free(fsCharset);

	fsCharset = strdup(charset);

	DEBUG("setFsCharset: fs charset is: %s\n",fsCharset);
	
	if(setCharSetConversion("UTF-8",fsCharset)!=0) {
		ERROR("fs charset conversion problem: "
			"not able to convert from \"%s\" to \"%s\"\n",
			fsCharset,"UTF-8");
	}
	if(setCharSetConversion(fsCharset,"UTF-8")!=0) {
		ERROR("fs charset conversion problem: "
			"not able to convert from \"%s\" to \"%s\"\n",
			"UTF-8",fsCharset);
	}
}

char * getFsCharset() {
	return fsCharset;
}

void initPaths() {
#ifdef HAVE_LOCALE
#ifdef HAVE_LANGINFO
	char * originalLocale;
#endif
#endif
	char * charset = NULL;

	if(getConf()[CONF_FS_CHARSET]) {
		charset = strdup(getConf()[CONF_FS_CHARSET]);
	}
#ifdef HAVE_LOCALE
#ifdef HAVE_LANGINFO
	else if((originalLocale = setlocale(LC_ALL,""))) {
		char * temp;

		if((temp = nl_langinfo(CODESET))) {
			charset = strdup(temp);
		}
		else ERROR("problems getting charset for locale\n");
		if(!setlocale(LC_ALL,originalLocale)) {
			ERROR("problems resetting locale with setlocale()\n");
		}
	}
#endif
#endif
	else ERROR("problems getting locale with setlocale()\n");

	if(charset) {
		setFsCharset(charset);
		free(charset);
	}
	else {
		ERROR("setting filesystem charset to UTF-8\n");
		setFsCharset("UTF-8");
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
	int i;

	memset(parentPath,0,MAXPATHLEN+1);

	strncpy(parentPath,path,MAXPATHLEN);
	while(strlen(parentPath) && parentPath[strlen(parentPath)-1]=='/') {
		parentPath[strlen(parentPath)-1] = '\0';
	}
	for(i=strlen(parentPath);i>=0;i--) {
		if(parentPath[i]=='/') break;
		parentPath[i] = '\0';
	}
	while(strlen(parentPath) && parentPath[strlen(parentPath)-1]=='/') {
		parentPath[strlen(parentPath)-1] = '\0';
	}

	return parentPath;
}
