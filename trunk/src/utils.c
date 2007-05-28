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

#include "utils.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <assert.h>

char *myFgets(char *buffer, int bufferSize, FILE * fp)
{
	char *ret = fgets(buffer, bufferSize, fp);
	if (ret && strlen(buffer) > 0 && buffer[strlen(buffer) - 1] == '\n') {
		buffer[strlen(buffer) - 1] = '\0';
	}
	if (ret && strlen(buffer) > 0 && buffer[strlen(buffer) - 1] == '\r') {
		buffer[strlen(buffer) - 1] = '\0';
	}
	return ret;
}

char *strDupToUpper(char *str)
{
	char *ret = xstrdup(str);
	int i;

	for (i = 0; i < strlen(str); i++)
		ret[i] = toupper((int)ret[i]);

	return ret;
}

void stripReturnChar(char *string)
{
	while (string && (string = strchr(string, '\n'))) {
		*string = ' ';
	}
}

void my_usleep(long usec)
{
	struct timeval tv;

	tv.tv_sec = 0;
	tv.tv_usec = usec;

	select(0, NULL, NULL, NULL, &tv);
}

int ipv6Supported(void)
{
#ifdef HAVE_IPV6
	int s;
	s = socket(AF_INET6, SOCK_STREAM, 0);
	if (s == -1)
		return 0;
	close(s);
	return 1;
#endif
	return 0;
}

char *appendToString(char *dest, const char *src)
{
	int destlen;
	int srclen = strlen(src);

	if (dest == NULL) {
		dest = xmalloc(srclen + 1);
		memset(dest, 0, srclen + 1);
		destlen = 0;
	} else {
		destlen = strlen(dest);
		dest = xrealloc(dest, destlen + srclen + 1);
	}

	memcpy(dest + destlen, src, srclen);
	dest[destlen + srclen] = '\0';

	return dest;
}

unsigned long readLEuint32(const unsigned char *p)
{
	return ((unsigned long)p[0] << 0) |
	    ((unsigned long)p[1] << 8) |
	    ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

mpd_malloc char *xstrdup(const char *s)
{
	char *ret = strdup(s);
	if (mpd_unlikely(!ret))
		FATAL("OOM: strdup\n");
	return ret;
}

/* borrowed from git :) */

mpd_malloc void *xmalloc(size_t size)
{
	void *ret;

	assert(mpd_likely(size));

	ret = malloc(size);
	if (mpd_unlikely(!ret))
		FATAL("OOM: malloc\n");
	return ret;
}

mpd_malloc void *xrealloc(void *ptr, size_t size)
{
	void *ret = realloc(ptr, size);

	/* some C libraries return NULL when size == 0,
	 * make sure we get a free()-able pointer (free(NULL)
	 * doesn't work with all C libraries, either) */
	if (mpd_unlikely(!ret && !size))
		ret = realloc(ptr, 1);

	if (mpd_unlikely(!ret))
		FATAL("OOM: realloc\n");
	return ret;
}

mpd_malloc void *xcalloc(size_t nmemb, size_t size)
{
	void *ret;

	assert(mpd_likely(nmemb && size));

	ret = calloc(nmemb, size);
	if (mpd_unlikely(!ret))
		FATAL("OOM: calloc\n");
	return ret;
}


