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
#include "list.h"
#include "stored_playlist.h"
#include "os_compat.h"

static const char *remoteUrlPrefixes[] = {
#ifdef HAVE_CURL
	"http://",
#endif
	NULL
};

void printRemoteUrlHandlers(struct client *client)
{
	const char **prefixes = remoteUrlPrefixes;

	while (*prefixes) {
		client_printf(client, "handler: %s\n", *prefixes);
		prefixes++;
	}
}


bool isRemoteUrl(const char *url)
{
	const char **urlPrefixes = remoteUrlPrefixes;

	while (*urlPrefixes) {
		if (g_str_has_prefix(url, *urlPrefixes))
			return true;
		urlPrefixes++;
	}

	return false;
}

/* suffixes should be ascii only characters */
const char *getSuffix(const char *utf8file)
{
	const char *dot = strrchr(utf8file, '.');

	return dot != NULL ? dot + 1 : NULL;
}

const struct decoder_plugin *
hasMusicSuffix(const char *utf8file, unsigned int next)
{
	const struct decoder_plugin *ret = NULL;

	const char *s = getSuffix(utf8file);
	if (s) {
		ret = decoder_plugin_from_suffix(s, next);
	} else {
		DEBUG("hasMusicSuffix: The file: %s has no valid suffix\n",
		      utf8file);
	}

	return ret;
}
