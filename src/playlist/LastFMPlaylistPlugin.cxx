/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "LastFMPlaylistPlugin.hxx"
#include "PlaylistPlugin.hxx"
#include "PlaylistRegistry.hxx"
#include "conf.h"
#include "Song.hxx"
#include "InputStream.hxx"
#include "util/Error.hxx"

#include <glib.h>

#include <assert.h>
#include <string.h>

struct LastfmPlaylist {
	struct playlist_provider base;

	struct input_stream *is;

	struct playlist_provider *xspf;

	LastfmPlaylist(input_stream *_is, playlist_provider *_xspf)
		:is(_is), xspf(_xspf) {
		playlist_provider_init(&base, &lastfm_playlist_plugin);
	}

	~LastfmPlaylist() {
		playlist_plugin_close(xspf);
		is->Close();
	}
};

static struct {
	char *user;
	char *md5;
} lastfm_config;

static bool
lastfm_init(const config_param &param)
{
	const char *user = param.GetBlockValue("user");
	const char *passwd = param.GetBlockValue("password");

	if (user == NULL || passwd == NULL) {
		g_debug("disabling the last.fm playlist plugin "
			"because account is not configured");
		return false;
	}

	lastfm_config.user = g_uri_escape_string(user, NULL, false);

	if (strlen(passwd) != 32)
		lastfm_config.md5 = g_compute_checksum_for_string(G_CHECKSUM_MD5,
								  passwd, strlen(passwd));
	else
		lastfm_config.md5 = g_strdup(passwd);

	return true;
}

static void
lastfm_finish(void)
{
	g_free(lastfm_config.user);
	g_free(lastfm_config.md5);
}

/**
 * Simple data fetcher.
 * @param url path or url of data to fetch.
 * @return data fetched, or NULL on error. Must be freed with g_free.
 */
static char *
lastfm_get(const char *url, Mutex &mutex, Cond &cond)
{
	struct input_stream *input_stream;
	Error error;
	char buffer[4096];
	size_t length = 0;

	input_stream = input_stream::Open(url, mutex, cond, error);
	if (input_stream == NULL) {
		if (error.IsDefined())
			g_warning("%s", error.GetMessage());

		return NULL;
	}

	mutex.lock();

	input_stream->WaitReady();

	do {
		size_t nbytes =
			input_stream->Read(buffer + length,
					   sizeof(buffer) - length, error);
		if (nbytes == 0) {
			if (error.IsDefined())
				g_warning("%s", error.GetMessage());

			if (input_stream->IsEOF())
				break;

			/* I/O error */
			mutex.unlock();
			input_stream->Close();
			return NULL;
		}

		length += nbytes;
	} while (length < sizeof(buffer));

	mutex.unlock();

	input_stream->Close();
	return g_strndup(buffer, length);
}

/**
 * Ini-style value fetcher.
 * @param response data through which to search.
 * @param name name of value to search for.
 * @return value for param name in param response or NULL on error. Free with g_free.
 */
static char *
lastfm_find(const char *response, const char *name)
{
	size_t name_length = strlen(name);

	while (true) {
		const char *eol = strchr(response, '\n');
		if (eol == NULL)
			return NULL;

		if (strncmp(response, name, name_length) == 0 &&
		    response[name_length] == '=') {
			response += name_length + 1;
			return g_strndup(response, eol - response);
		}

		response = eol + 1;
	}
}

static struct playlist_provider *
lastfm_open_uri(const char *uri, Mutex &mutex, Cond &cond)
{
	char *p, *q, *response, *session;

	/* handshake */

	p = g_strconcat("http://ws.audioscrobbler.com/radio/handshake.php?"
			"version=1.1.1&platform=linux&"
			"username=", lastfm_config.user, "&"
			"passwordmd5=", lastfm_config.md5, "&"
			"debug=0&partner=", NULL);
	response = lastfm_get(p, mutex, cond);
	g_free(p);
	if (response == NULL)
		return NULL;

	/* extract session id from response */

	session = lastfm_find(response, "session");
	g_free(response);
	if (session == NULL) {
		g_warning("last.fm handshake failed");
		return NULL;
	}

	q = g_uri_escape_string(session, NULL, false);
	g_free(session);
	session = q;

	g_debug("session='%s'", session);

	/* "adjust" last.fm radio */

	if (strlen(uri) > 9) {
		char *escaped_uri;

		escaped_uri = g_uri_escape_string(uri, NULL, false);

		p = g_strconcat("http://ws.audioscrobbler.com/radio/adjust.php?"
				"session=", session, "&url=", escaped_uri, "&debug=0",
				NULL);
		g_free(escaped_uri);

		response = lastfm_get(p, mutex, cond);
		g_free(response);
		g_free(p);

		if (response == NULL) {
			g_free(session);
			return NULL;
		}
	}

	/* open the last.fm playlist */

	p = g_strconcat("http://ws.audioscrobbler.com/radio/xspf.php?"
			"sk=", session, "&discovery=0&desktop=1.5.1.31879",
			NULL);
	g_free(session);

	Error error;
	const auto is = input_stream::Open(p, mutex, cond, error);
	g_free(p);

	if (is == nullptr) {
		if (error.IsDefined())
			g_warning("Failed to load XSPF playlist: %s",
				  error.GetMessage());
		else
			g_warning("Failed to load XSPF playlist");
		return NULL;
	}

	mutex.lock();

	is->WaitReady();

	/* last.fm does not send a MIME type, we have to fake it here
	   :-( */
	is->OverrideMimeType("application/xspf+xml");

	mutex.unlock();

	/* parse the XSPF playlist */

	const auto xspf = playlist_list_open_stream(is, nullptr);
	if (xspf == nullptr) {
		is->Close();
		g_warning("Failed to parse XSPF playlist");
		return NULL;
	}

	/* create the playlist object */

	const auto playlist = new LastfmPlaylist(is, xspf);
	return &playlist->base;
}

static void
lastfm_close(struct playlist_provider *_playlist)
{
	LastfmPlaylist *playlist = (LastfmPlaylist *)_playlist;

	delete playlist;
}

static Song *
lastfm_read(struct playlist_provider *_playlist)
{
	LastfmPlaylist *playlist = (LastfmPlaylist *)_playlist;

	return playlist_plugin_read(playlist->xspf);
}

static const char *const lastfm_schemes[] = {
	"lastfm",
	NULL
};

const struct playlist_plugin lastfm_playlist_plugin = {
	"lastfm",

	lastfm_init,
	lastfm_finish,
	lastfm_open_uri,
	nullptr,
	lastfm_close,
	lastfm_read,

	lastfm_schemes,
	nullptr,
	nullptr,
};
