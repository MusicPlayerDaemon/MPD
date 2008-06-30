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

/* This file is only included by inputStream_http.c */
#ifndef INPUT_STREAM_HTTP_AUTH_H
#define INPUT_STREAM_HTTP_AUTH_H

#include "os_compat.h"
#include "utils.h"

/* base64 code taken from xmms */

#define BASE64_LENGTH(len) (4 * (((len) + 2) / 3))

static char *base64Dup(char *s)
{
	int i;
	int len = strlen(s);
	char *ret = xcalloc(BASE64_LENGTH(len) + 1, 1);
	unsigned char *p = (unsigned char *)ret;

	static const char tbl[64] = {
		'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
		'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
		'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
		'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
		'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
		'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
		'w', 'x', 'y', 'z', '0', '1', '2', '3',
		'4', '5', '6', '7', '8', '9', '+', '/'
	};

	/* Transform the 3x8 bits to 4x6 bits, as required by base64.  */
	for (i = 0; i < len; i += 3) {
		*p++ = tbl[s[0] >> 2];
		*p++ = tbl[((s[0] & 3) << 4) + (s[1] >> 4)];
		*p++ = tbl[((s[1] & 0xf) << 2) + (s[2] >> 6)];
		*p++ = tbl[s[2] & 0x3f];
		s += 3;
	}
	/* Pad the result if necessary...  */
	if (i == len + 1)
		*(p - 1) = '=';
	else if (i == len + 2)
		*(p - 1) = *(p - 2) = '=';
	/* ...and zero-terminate it.  */
	*p = '\0';

	return ret;
}

static char *authString(const char *header,
			const char *user, const char *password)
{
	char *ret = NULL;
	int templen;
	char *temp;
	char *temp64;

	if (!user || !password)
		return NULL;

	templen = strlen(user) + strlen(password) + 2;
	temp = xmalloc(templen);
	strcpy(temp, user);
	strcat(temp, ":");
	strcat(temp, password);
	temp64 = base64Dup(temp);
	free(temp);

	ret = xmalloc(strlen(temp64) + strlen(header) + 3);
	strcpy(ret, header);
	strcat(ret, temp64);
	strcat(ret, "\r\n");
	free(temp64);

	return ret;
}

#define PROXY_AUTH_HEADER	"Proxy-Authorization: Basic "
#define HTTP_AUTH_HEADER	"Authorization: Basic "

#define proxyAuthString(x, y)	authString(PROXY_AUTH_HEADER, x, y)
#define httpAuthString(x, y)	authString(HTTP_AUTH_HEADER, x, y)

#endif /* INPUT_STREAM_HTTP_AUTH_H */
