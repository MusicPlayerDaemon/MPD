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

#include "ls.h"
#include "playlist.h"
#include "path.h"
#include "client.h"
#include "log.h"
#include "utils.h"
#include "list.h"
#include "stored_playlist.h"
#include "os_compat.h"

static const char *remoteUrlPrefixes[] = {
#ifdef HAVE_CURL
	"http://",
#endif
	NULL
};

int printRemoteUrlHandlers(struct client *client)
{
	const char **prefixes = remoteUrlPrefixes;

	while (*prefixes) {
		client_printf(client, "handler: %s\n", *prefixes);
		prefixes++;
	}

	return 0;
}

int isValidRemoteUtf8Url(const char *utf8url)
{
	int ret = 0;
	const char *temp;

	switch (isRemoteUrl(utf8url)) {
	case 1:
		ret = 1;
		temp = utf8url;
		while (*temp) {
			if ((*temp >= 'a' && *temp <= 'z') ||
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
			    *temp == ';' || *temp == '&' || *temp == '=') {
			} else {
				ret = 1;
				break;
			}
			temp++;
		}
		break;
	}

	return ret;
}

int isRemoteUrl(const char *url)
{
	int count = 0;
	const char **urlPrefixes = remoteUrlPrefixes;

	while (*urlPrefixes) {
		count++;
		if (!prefixcmp(url, *urlPrefixes))
			return count;
		urlPrefixes++;
	}

	return 0;
}

/* suffixes should be ascii only characters */
const char *getSuffix(const char *utf8file)
{
	const char *ret = NULL;

	while (*utf8file) {
		if (*utf8file == '.')
			ret = utf8file + 1;
		utf8file++;
	}

	return ret;
}

struct decoder_plugin *hasMusicSuffix(const char *utf8file, unsigned int next)
{
	struct decoder_plugin *ret = NULL;

	const char *s = getSuffix(utf8file);
	if (s) {
		ret = decoder_plugin_from_suffix(s, next);
	} else {
		DEBUG("hasMusicSuffix: The file: %s has no valid suffix\n",
		      utf8file);
	}

	return ret;
}
