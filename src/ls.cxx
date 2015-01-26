/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
 * http://www.musicpd.org
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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "ls.hxx"
#include "util/StringUtil.hxx"
#include "util/UriUtil.hxx"
#include "client/Client.hxx"

#include <assert.h>

/**
  * file:// is not included in remoteUrlPrefixes, the connection method
  * is detected at runtime and displayed as a urlhandler if the client is
  * connected by IPC socket.
  */
static const char *remoteUrlPrefixes[] = {
#if defined(ENABLE_CURL)
	"http://",
	"https://",
#endif
#ifdef ENABLE_MMS
	"mms://",
	"mmsh://",
	"mmst://",
	"mmsu://",
#endif
#ifdef HAVE_FFMPEG
	"gopher://",
	"rtp://",
	"rtsp://",
	"rtmp://",
	"rtmpt://",
	"rtmps://",
#endif
#ifdef ENABLE_SMBCLIENT
	"smb://",
#endif
#ifdef ENABLE_NFS
	"nfs://",
#endif
#ifdef ENABLE_CDIO_PARANOIA
	"cdda://",
#endif
#ifdef HAVE_ALSA
	"alsa://",
#endif
	NULL
};

void print_supported_uri_schemes_to_fp(FILE *fp)
{
	const char **prefixes = remoteUrlPrefixes;

#ifdef HAVE_UN
	fprintf(fp, " file://");
#endif
	while (*prefixes) {
		fprintf(fp, " %s", *prefixes);
		prefixes++;
	}
	fprintf(fp,"\n");
}

void print_supported_uri_schemes(Client &client)
{
	const char **prefixes = remoteUrlPrefixes;

	while (*prefixes) {
		client_printf(client, "handler: %s\n", *prefixes);
		prefixes++;
	}
}

bool uri_supported_scheme(const char *uri)
{
	const char **urlPrefixes = remoteUrlPrefixes;

	assert(uri_has_scheme(uri));

	while (*urlPrefixes) {
		if (StringStartsWith(uri, *urlPrefixes))
			return true;
		urlPrefixes++;
	}

	return false;
}
