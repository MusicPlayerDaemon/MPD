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

#include "utf8.h"
#include "utils.h"
#include "os_compat.h"

char *latin1_to_utf8(char *dest, char *in_latin1)
{
	unsigned char *cp = (unsigned char *)dest;
	unsigned char *latin1 = (unsigned char *)in_latin1;

	while (*latin1) {
		if (*latin1 < 128)
			*(cp++) = *latin1;
		else {
			if (*latin1 < 192) {
				*(cp++) = 194;
				*(cp++) = *latin1;
			} else {
				*(cp++) = 195;
				*(cp++) = (*latin1) - 64;
			}
		}
		++latin1;
	}

	*cp = '\0';

	return dest;
}

char *latin1StrToUtf8Dup(char *latin1)
{
	/* utf8 should have at most two char's per latin1 char */
	char *ret = xmalloc(strlen(latin1) * 2 + 1);

	ret = latin1_to_utf8(ret, latin1);

	return ((ret) ? xrealloc(ret, strlen((char *)ret) + 1) : NULL);
}

static char utf8_to_latin1_char(char *inUtf8)
{
	unsigned char c = 0;
	unsigned char *utf8 = (unsigned char *)inUtf8;

	if (utf8[0] < 128)
		return utf8[0];
	else if (utf8[0] == 195)
		c += 64;
	else if (utf8[0] != 194)
		return '?';
	return (char)(c + utf8[1]);
}

static int validateUtf8Char(char *inUtf8Char)
{
	unsigned char *utf8Char = (unsigned char *)inUtf8Char;

	if (utf8Char[0] < 0x80)
		return 1;

	if (utf8Char[0] >= 0xC0 && utf8Char[0] <= 0xFD) {
		int count = 1;
		char t = 1 << 5;
		int i;
		while (count < 6 && (t & utf8Char[0])) {
			t = (t >> 1);
			count++;
		}
		if (count > 5)
			return 0;
		for (i = 1; i <= count; i++) {
			if (utf8Char[i] < 0x80 || utf8Char[i] > 0xBF)
				return 0;
		}
		return count + 1;
	} else
		return 0;
}

int validUtf8String(char *string)
{
	int ret;

	while (*string) {
		ret = validateUtf8Char(string);
		if (0 == ret)
			return 0;
		string += ret;
	}

	return 1;
}

char *utf8StrToLatin1Dup(char *utf8)
{
	/* utf8 should have at most two char's per latin1 char */
	char *ret = xmalloc(strlen(utf8) + 1);
	char *cp = ret;
	int count;
	size_t len = 0;

	while (*utf8) {
		count = validateUtf8Char(utf8);
		if (!count) {
			free(ret);
			return NULL;
		}
		*(cp++) = utf8_to_latin1_char(utf8);
		utf8 += count;
		len++;
	}

	*cp = '\0';

	return xrealloc(ret, len + 1);
}

char *utf8_to_latin1(char *dest, char *utf8)
{
	char *cp = dest;
	int count;
	size_t len = 0;

	while (*utf8) {
		count = validateUtf8Char(utf8);
		if (count) {
			*(cp++) = utf8_to_latin1_char(utf8);
			utf8 += count;
			len++;
		} else
			return NULL;
	}

	*cp = '\0';
	return dest;
}
