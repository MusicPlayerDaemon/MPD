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

char *latin1_to_utf8(char *dest, const char *in_latin1)
{
	unsigned char *cp = (unsigned char *)dest;
	const unsigned char *latin1 = (const unsigned char *)in_latin1;

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

char *latin1StrToUtf8Dup(const char *latin1)
{
	/* utf8 should have at most two char's per latin1 char */
	char *ret = xmalloc(strlen(latin1) * 2 + 1);

	ret = latin1_to_utf8(ret, latin1);

	return ((ret) ? xrealloc(ret, strlen((char *)ret) + 1) : NULL);
}

static char utf8_to_latin1_char(const char *inUtf8)
{
	unsigned char c = 0;
	const unsigned char *utf8 = (const unsigned char *)inUtf8;

	if (utf8[0] < 128)
		return utf8[0];
	else if (utf8[0] == 195)
		c += 64;
	else if (utf8[0] != 194)
		return '?';
	return (char)(c + utf8[1]);
}

static unsigned int validateUtf8Char(const char *inUtf8Char, size_t length)
{
	const unsigned char *utf8Char = (const unsigned char *)inUtf8Char;

	assert(length > 0);

	if (utf8Char[0] < 0x80)
		return 1;

	if (utf8Char[0] >= 0xC0 && utf8Char[0] <= 0xFD) {
		unsigned int count = 1;
		char t = 1 << 5;
		unsigned int i;
		while (count < 6 && (t & utf8Char[0])) {
			t = (t >> 1);
			count++;
		}
		if (count > 5 || (size_t)count > length)
			return 0;
		for (i = 1; i <= count; i++) {
			if (utf8Char[i] < 0x80 || utf8Char[i] > 0xBF)
				return 0;
		}
		return count + 1;
	} else
		return 0;
}

int validUtf8String(const char *string, size_t length)
{
	unsigned int ret;

	while (length > 0) {
		ret = validateUtf8Char(string, length);
		assert((size_t)ret <= length);
		if (0 == ret)
			return 0;
		string += ret;
		length -= ret;
	}

	return 1;
}

char *utf8StrToLatin1Dup(const char *utf8)
{
	/* utf8 should have at most two char's per latin1 char */
	char *ret = xmalloc(strlen(utf8) + 1);
	char *cp = ret;
	unsigned int count;
	size_t len = 0;

	while (*utf8) {
		count = validateUtf8Char(utf8, INT_MAX);
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

char *utf8_to_latin1(char *dest, const char *utf8)
{
	char *cp = dest;
	unsigned int count;
	size_t len = 0;

	while (*utf8) {
		count = validateUtf8Char(utf8, INT_MAX);
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
