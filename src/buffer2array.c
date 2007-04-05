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

#include "buffer2array.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


static inline
int
isWhiteSpace(char c)
{
	return (c == ' ' || c == '\t');
}

int buffer2array(char *buffer, char *array[], const int max)
{
	int i = 0;
	char *c = buffer;

	while (*c != '\0' && i < max) {
		if (*c == '\"') {
			array[i++] = ++c;
			while (*c != '\0') {
				if (*c == '\"') {
					*(c++) = '\0';
					break;
				}
				else if (*(c++) == '\\' && *c != '\0') {
					memmove(c - 1, c, strlen(c) + 1);
				}
			}
		} else {
			while (isWhiteSpace(*c))
				++c;
			array[i++] = c++;
			if (*c == '\0')
				return i;
			while (!isWhiteSpace(*c) && *c != '\0')
				++c;
		}
		if (*c == '\0')
			return i;
		*(c++) = '\0';
		while (isWhiteSpace(*c))
			++c;
	}
	return i;
}

#ifdef UNIT_TEST

#include <stdio.h>
#include <string.h>
#include <assert.h>

int main()
{
	char *a[4] = { NULL };
	char *b;
	int max;

	b = strdup("lsinfo \"/some/dir/name \\\"test\\\"\"");
	max = buffer2array(b, a, 4);
	assert( !strcmp("lsinfo", a[0]) );
	assert( !strcmp("/some/dir/name \"test\"", a[1]) );
	assert( !a[2] );

	b = strdup("lsinfo \"/some/dir/name \\\"test\\\" something else\"");
	max = buffer2array(b, a, 4);
	assert( !strcmp("lsinfo", a[0]) );
	assert( !strcmp("/some/dir/name \"test\" something else", a[1]) );
	assert( !a[2] );

	b = strdup("lsinfo \"/some/dir\\\\name\"");
	max = buffer2array(b, a, 4);
	assert( !strcmp("lsinfo", a[0]) );
	assert( !strcmp("/some/dir\\name", a[1]) );
	assert( !a[2] );

	b = strdup("lsinfo \"/some/dir name\"");
	max = buffer2array(b, a, 4);
	assert( !strcmp("lsinfo", a[0]) );
	assert( !strcmp("/some/dir name", a[1]) );
	assert( !a[2] );

	b = strdup("lsinfo \"\\\"/some/dir\\\"\"");
	max = buffer2array(b, a, 4);
	assert( !strcmp("lsinfo", a[0]) );
	assert( !strcmp("\"/some/dir\"", a[1]) );
	assert( !a[2] );

	b = strdup("lsinfo \"\\\"/some/dir\\\" x\"");
	max = buffer2array(b, a, 4);
	assert( !strcmp("lsinfo", a[0]) );
	assert( !strcmp("\"/some/dir\" x", a[1]) );
	assert( !a[2] );

	b = strdup("lsinfo \"single quote\\'d from php magicquotes\"");
	max = buffer2array(b, a, 4);
	assert( !strcmp("lsinfo", a[0]) );
	assert( !strcmp("single quote\'d from php magicquotes", a[1]) );
	assert( !a[2] );

	b = strdup("lsinfo \"double quote\\\"d from php magicquotes\"");
	max = buffer2array(b, a, 4);
	assert( !strcmp("lsinfo", a[0]) );
	assert( !strcmp("double quote\"d from php magicquotes", a[1]) );
	assert( !a[2] );

	return 0;
}

#endif
