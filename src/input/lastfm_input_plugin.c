/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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

#include "input/lastfm_input_plugin.h"
#include "input/curl_input_plugin.h"
#include "input_plugin.h"
#include "conf.h"

#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "input_lastfm"

static const char *lastfm_user, *lastfm_password;

static bool
lastfm_input_init(const struct config_param *param)
{
	lastfm_user = config_get_block_string(param, "user", NULL);
	lastfm_password = config_get_block_string(param, "password", NULL);

	return lastfm_user != NULL && lastfm_password != NULL;
}

static char *
lastfm_get(const char *url)
{
	struct input_stream input_stream;
	bool success;
	int ret;
	char buffer[4096];
	size_t length = 0, nbytes;

	success = input_stream_open(&input_stream, url);
	if (!success)
		return NULL;

	while (!input_stream.ready) {
		ret = input_stream_buffer(&input_stream);
		if (ret < 0) {
			input_stream_close(&input_stream);
			return NULL;
		}
	}

	do {
		nbytes = input_stream_read(&input_stream, buffer + length,
					   sizeof(buffer) - length);
		if (nbytes == 0) {
			if (input_stream_eof(&input_stream))
				break;

			/* I/O error */
			input_stream_close(&input_stream);
			return NULL;
		}

		length += nbytes;
	} while (length < sizeof(buffer));

	input_stream_close(&input_stream);
	return g_strndup(buffer, length);
}

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

static bool
lastfm_input_open(struct input_stream *is, const char *url)
{
	char *md5, *p, *q, *response, *session, *stream_url;
	bool success;

	if (strncmp(url, "lastfm://", 9) != 0)
		return false;

	/* handshake */

#if GLIB_CHECK_VERSION(2,16,0)
	q = g_uri_escape_string(lastfm_user, NULL, false);
#else
	q = g_strdup(lastfm_username);
#endif

#if GLIB_CHECK_VERSION(2,16,0)
	if (strlen(lastfm_password) != 32)
		md5 = g_compute_checksum_for_string(G_CHECKSUM_MD5,
						    lastfm_password,
						    strlen(lastfm_password));
	else
#endif
		md5 = g_strdup(lastfm_password);

	p = g_strconcat("http://ws.audioscrobbler.com/radio/handshake.php?"
			"version=1.1.1&platform=linux&"
			"username=", q, "&"
			"passwordmd5=", md5, "&debug=0&partner=", NULL);
	g_free(q);
	g_free(md5);

	response = lastfm_get(p);
	g_free(p);
	if (response == NULL)
		return false;

	/* extract session id from response */

	session = lastfm_find(response, "session");
	stream_url = lastfm_find(response, "stream_url");
	g_free(response);
	if (session == NULL || stream_url == NULL) {
		g_free(session);
		g_free(stream_url);
		return false;
	}

#if GLIB_CHECK_VERSION(2,16,0)
		q = g_uri_escape_string(session, NULL, false);
		g_free(session);
		session = q;
#endif

	/* "adjust" last.fm radio */

	if (strlen(url) > 9) {
		char *escaped_url;

#if GLIB_CHECK_VERSION(2,16,0)
		escaped_url = g_uri_escape_string(url, NULL, false);
#else
		escaped_url = g_strdup(url);
#endif

		p = g_strconcat("http://ws.audioscrobbler.com/radio/adjust.php?"
				"session=", session, "&url=", escaped_url, "&debug=0",
				NULL);
		g_free(escaped_url);

		response = lastfm_get(p);
		g_free(response);
		g_free(p);

		if (response == NULL) {
			g_free(session);
			g_free(stream_url);
			return false;
		}
	}

	/* load the last.fm playlist */

	p = g_strconcat("http://ws.audioscrobbler.com/radio/xspf.php?"
			"sk=", session, "&discovery=0&desktop=1.5.1.31879",
			NULL);
	g_free(session);

	response = lastfm_get(p);
	g_free(p);

	if (response == NULL) {
		g_free(stream_url);
		return false;
	}

	p = strstr(response, "<location>");
	if (p == NULL) {
		g_free(response);
		g_free(stream_url);
		return false;
	}

	p += 10;
	q = strchr(p, '<');

	if (q == NULL) {
		g_free(response);
		g_free(stream_url);
		return false;
	}

	g_free(stream_url);
	stream_url = g_strndup(p, q - p);
	g_free(response);

	/* now really open the last.fm radio stream */

	success = input_stream_open(is, stream_url);
	g_free(stream_url);
	return success;
}

const struct input_plugin lastfm_input_plugin = {
	.name = "lastfm",
	.init = lastfm_input_init,
	.open = lastfm_input_open,
};
