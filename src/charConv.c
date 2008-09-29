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

#include "charConv.h"
#include "utf8.h"
#include "utils.h"
#include "path.h"
#include "os_compat.h"

#include "../config.h"

#ifdef HAVE_ICONV
#include <iconv.h>
static iconv_t char_conv_iconv;
#endif

static char *char_conv_to;
static char *char_conv_from;
static int8_t char_conv_same;
static int8_t char_conv_use_iconv;

/* 1 is to use latin1ToUtf8
   0 is not to use latin1/utf8 converter
  -1 is to use utf8ToLatin1*/
static int8_t char_conv_latin1ToUtf8;

#define BUFFER_SIZE	MPD_PATH_MAX

static void closeCharSetConversion(void);

int setCharSetConversion(const char *to, const char *from)
{
	if (char_conv_to && char_conv_from) {
		if (char_conv_latin1ToUtf8 &&
		    !strcmp(from, char_conv_to) &&
		    !strcmp(to, char_conv_from)) {
			char *swap = char_conv_from;
			char_conv_from = char_conv_to;
			char_conv_to = swap;
			char_conv_latin1ToUtf8 *= -1;
			return 0;
		} else if (!strcmp(to, char_conv_to) &&
		           !strcmp(from,char_conv_from)) {
			return 0;
		}
	}

	closeCharSetConversion();

	if (0 == strcmp(to, from)) {
		char_conv_same = 1;
		char_conv_to = xstrdup(to);
		char_conv_from = xstrdup(from);
		return 0;
	}

	if (strcmp(to, "UTF-8") == 0 && strcmp(from, "ISO-8859-1") == 0) {
		char_conv_latin1ToUtf8 = 1;
	} else if (strcmp(to, "ISO-8859-1") == 0 && strcmp(from, "UTF-8") == 0) {
		char_conv_latin1ToUtf8 = -1;
	}

	if (char_conv_latin1ToUtf8 != 0) {
		char_conv_to = xstrdup(to);
		char_conv_from = xstrdup(from);
		return 0;
	}
#ifdef HAVE_ICONV
	if ((char_conv_iconv = iconv_open(to, from)) == (iconv_t) (-1))
		return -1;

	char_conv_to = xstrdup(to);
	char_conv_from = xstrdup(from);
	char_conv_use_iconv = 1;

	return 0;
#else
	return -1;
#endif
}

#ifdef HAVE_ICONV
static inline size_t deconst_iconv(iconv_t cd,
				   const char **inbuf, size_t *inbytesleft,
				   char **outbuf, size_t *outbytesleft)
{
	union {
		const char **a;
		char **b;
	} deconst;

	deconst.a = inbuf;

	return iconv(cd, deconst.b, inbytesleft, outbuf, outbytesleft);
}
#endif

char *char_conv_str(char *dest, const char *string)
{
	if (!char_conv_to)
		return NULL;

	if (char_conv_same)
		return strcpy(dest, string);

#ifdef HAVE_ICONV
	if (char_conv_use_iconv) {
		/* not optimized: */
		char buffer[BUFFER_SIZE];
		size_t inleft = strlen(string);
		size_t outleft;
		size_t retlen = 0;
		size_t err;
		char *bufferPtr;

		dest[0] = '\0';

		while (inleft) {
			bufferPtr = buffer;
			outleft = BUFFER_SIZE;
			err =
			    deconst_iconv(char_conv_iconv, &string, &inleft,
					  &bufferPtr, &outleft);
			if (outleft == BUFFER_SIZE
			    || (err == (size_t)-1L && errno != E2BIG)) {
				return NULL;
			}
			memcpy(dest + retlen, buffer, BUFFER_SIZE - outleft);
			retlen += BUFFER_SIZE - outleft;
			dest[retlen] = '\0';
		}

		return dest;
	}
#endif

	switch (char_conv_latin1ToUtf8) {
	case 1:
		return latin1_to_utf8(dest, string);
	case -1:
		return utf8_to_latin1(dest, string);
	}

	return NULL;
}

static void closeCharSetConversion(void)
{
	if (char_conv_to) {
#ifdef HAVE_ICONV
		if (char_conv_use_iconv)
			iconv_close(char_conv_iconv);
#endif
		free(char_conv_to);
		free(char_conv_from);
		char_conv_to = NULL;
		char_conv_from = NULL;
		char_conv_same = 0;
		char_conv_latin1ToUtf8 = 0;
		char_conv_use_iconv = 0;
	}
}
