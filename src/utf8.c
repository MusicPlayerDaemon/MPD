/* the Music Player Daemon (MPD)
 * (c)2003-2004 by Warren Dukes (shank@mercury.chem.pitt.edu
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

#include "utf8.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

unsigned char * latin1ToUtf8(unsigned char c) {
	static unsigned char utf8[3];

	memset(utf8,0,3);

	if(c < 128) utf8[0] = c;
	else if(c<192) {
		utf8[0] = 194;
		utf8[1] = c;
	}
	else {
		utf8[0] = 195;
		utf8[1] = c-64;
	}

	return utf8;
}

unsigned char * latin1StrToUtf8Dup(unsigned char * latin1) {
	/* utf8 should have at most two char's per latin1 char */
	int len = strlen(latin1)*2+1;
	unsigned char * ret = malloc(len);
	unsigned char * cp = ret;
	unsigned char * utf8;

	memset(ret,0,len);

	len = 0;

	while(*latin1) {
		utf8 = latin1ToUtf8(*latin1);
		while(*utf8) {
			*(cp++) = *(utf8++);
			len++;
		}
		latin1++;
	}

	return realloc(ret,len+1);
}

unsigned char utf8ToLatin1(unsigned char * utf8) {
	unsigned char c = 0;

	if(utf8[0]<128) return utf8[0];
	else if(utf8[0]==195) c+=64;
	else if(utf8[0]!=194) return '?';
	return c+utf8[1];
}

int validateUtf8Char(unsigned char * utf8Char) {
	if(utf8Char[0]<0x80) return 1;
	
	if(utf8Char[0]>=0xC0 && utf8Char[0]<=0xFD) {
		int count = 1;
		unsigned char t = 1 << 5;
		int i;
		while(count < 6 && (t & utf8Char[0])) {
			t = (t >> 1);
			count++;
		}
		if(count > 5) return 0;
		for(i=1;i<=count;i++) {
			if(utf8Char[i] < 0x80 || utf8Char[i] > 0xBF) return 0;
		}
		return count+1;
	}
	else return 0;
}

int validUtf8String(unsigned char * string) {
	int ret;

	while(*string) {
		ret = validateUtf8Char(string);
		if(0==ret) return 0;
		string+= ret;
	}

	return 1;
}

unsigned char * utf8StrToLatin1Dup(unsigned char * utf8) {
	/* utf8 should have at most two char's per latin1 char */
	int len = strlen(utf8)+1;
	unsigned char * ret = malloc(len);
	unsigned char * cp = ret;
	int count;

	memset(ret,0,len);

	len = 0;

	while(*utf8) {
		count = validateUtf8Char(utf8);
		if(!count) {
			free(ret);
			return NULL;
		}
		*(cp++) = utf8ToLatin1(utf8);
		utf8+= count;
		len++;
	}

	return realloc(ret,len+1);
}
/* vim:set shiftwidth=4 tabstop=8 expandtab: */
