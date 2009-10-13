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
#include "input_plugin.h"
#include "tag.h"
#include "conf.h"

#include <stdlib.h>
#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "input_lastfm"

static struct lastfm_data {
	char *user;
	char *md5;
} lastfm_data;

struct lastfm_input {
	/* our very own plugin wrapper */
	struct input_plugin wrap_plugin;

	/* pointer to input stream's plugin */
	const struct input_plugin *plugin;

	/* pointer to input stream's data */
	void *data;

	/* current track's tag */
	struct tag *tag;
};

static bool
lastfm_input_init(const struct config_param *param)
{
	const char *passwd = config_get_block_string(param, "password", NULL);
	const char *user = config_get_block_string(param, "user", NULL);
	if (passwd == NULL || user == NULL)
		return false;

	lastfm_data.user = g_uri_escape_string(user, NULL, false);

	if (strlen(passwd) != 32)
		lastfm_data.md5 = g_compute_checksum_for_string(G_CHECKSUM_MD5,
						    passwd, strlen(passwd));
	else
		lastfm_data.md5 = g_strdup(passwd);

	return true;
}

static void
lastfm_input_finish(void)
{
	g_free(lastfm_data.user);
	g_free(lastfm_data.md5);
}

/**
 * Simple data fetcher.
 * @param url path or url of data to fetch.
 * @return data fetched, or NULL on error. Must be freed with g_free.
 */
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

/**
 * Ini-style value fetcher.
 * @param response data through which to search.
 * @param name name of value to search for.
 * @return value for param name in param reponse or NULL on error. Free with g_free.
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

/**
 * Replace XML's five predefined entities with the equivalant characters.
 * glib doesn't seem to have code to do this, even in the xml parser.
 * We don't manage numerical character references such as &#nnnn; or &#xhhhh;.
 * @param value XML text to decode.
 * @return decoded string, which must be freed with g_free.
 * @todo make sure this is ok for utf-8.
 */
static char *
lastfm_xmldecode(const char *value)
{
	struct entity {
		const char *text;
		char repl;
	} entities[] = {
		{"&amp;",  '&'},
		{"&quot;", '"'},
		{"&apos;", '\''},
		{"&gt;",   '>'},
		{"&lt;",   '<'}
	};
	char *txt = g_strdup(value);
	unsigned int i;

	for (i = 0; i < sizeof(entities)/sizeof(entities[0]); ++i) {
		char *p;
		int slen = strlen(entities[i].text);
		while ((p = strstr(txt, entities[i].text))) {
			*p = entities[i].repl;
			g_strlcpy(p + 1, p + slen, strlen(p) - slen);
		}
	}
	return txt;
}

/**
 * Extract the text between xml start and end tags specified by param tag.
 * Caveat: This function does not handle nested tags properly.
 * @param response XML to extract text from.
 * @param tag name of tag of which text should be extracted from.
 * @return text between tags specified by param tag, NULL on error; Must be freed with g_free.
 */
static char *
lastfm_xmltag(const char *response, const char *tag)
{
	char *tn = g_strconcat("<", tag, ">", NULL);
	char *p, *q;

	if (!(p = strstr(response, tn))) {
		g_free(tn);
		return NULL;
	}

	p += strlen(tn);
	g_free(tn);

	tn = g_strconcat("</", tag, ">", NULL);

	if (!(q = strstr(p, tn))) {
		g_free(tn);
		return NULL;
	}

	g_free(tn);

	return g_strndup(p, q - p);
}

/**
 * Parses xspf track and generates mpd tag.
 * @return tag which must be freed with tag_free.
 */
static struct tag *
lastfm_read_tag(const char *response)
{
	struct tagalias {
		enum tag_type type;
		const char *xmltag;
	} aliases[] = {
		{TAG_ITEM_ARTIST, "creator"},
		{TAG_ITEM_TITLE, "title"},
		{TAG_ITEM_ALBUM, "album"}
	};
	struct tag *tag = tag_new();
	unsigned int i;
	char *track_time = lastfm_xmltag(response, "duration");

	if (track_time != NULL) {
		int mtime = strtol(track_time, 0, 0);
		g_free(track_time);

		/* make sure to round up */
		tag->time = ((mtime + 999) / 1000);
	}
	else
		tag->time = 0;

	for (i = 0; i < sizeof(aliases)/sizeof(aliases[0]); ++i) {
		char *p, *value = lastfm_xmltag(response, aliases[i].xmltag);
		if (value == NULL)
			continue;

		p = lastfm_xmldecode(value);
		g_free(value);
		value = p;

		tag_add_item(tag, aliases[i].type, value);
		g_free(value);
	}
	return tag;
}

static size_t
lastfm_input_read_wrap(struct input_stream *is, void *ptr, size_t size)
{
	size_t ret;
	struct lastfm_input *d = is->data;
	is->data = d->data;
	ret = (* d->plugin->read)(is, ptr, size);
	is->data = d;
	return ret;
}

static bool
lastfm_input_eof_wrap(struct input_stream *is)
{
	bool ret;
	struct lastfm_input *d = is->data;
	is->data = d->data;
	ret = (* d->plugin->eof)(is);
	is->data = d;
	return ret;
}

static bool
lastfm_input_seek_wrap(struct input_stream *is, goffset offset, int whence)
{
	bool ret;
	struct lastfm_input *d = is->data;
	is->data = d->data;
	ret = (* d->plugin->seek)(is, offset, whence);
	is->data = d;
	return ret;
}

static int
lastfm_input_buffer_wrap(struct input_stream *is)
{
	int ret;
	struct lastfm_input *d = is->data;
	is->data = d->data;
	ret = (* d->plugin->buffer)(is);
	is->data = d;
	return ret;
}

static struct tag *
lastfm_input_tag(struct input_stream *is)
{
	struct lastfm_input *d = is->data;
	struct tag *tag = d->tag;
	d->tag = NULL;
	return tag;
}

static void
lastfm_input_close(struct input_stream *is)
{
	struct lastfm_input *d = is->data;

	if (is->plugin->close) {
		is->data = d->data;
		is->plugin = d->plugin;
		(* is->plugin->close)(is);
	}

	if (d->tag)
		tag_free(d->tag);
	g_free(d);
}

static bool
lastfm_input_open(struct input_stream *is, const char *url)
{
	char *p, *q, *response, *track, *session, *stream_url;
	bool success;

	if (strncmp(url, "lastfm://", 9) != 0)
		return false;

	/* handshake */

	p = g_strconcat("http://ws.audioscrobbler.com/radio/handshake.php?"
			"version=1.1.1&platform=linux&"
			"username=", lastfm_data.user, "&"
			"passwordmd5=", lastfm_data.md5, "&"
			"debug=0&partner=", NULL);

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

	q = g_uri_escape_string(session, NULL, false);
	g_free(session);
	session = q;

	/* "adjust" last.fm radio */

	if (strlen(url) > 9) {
		char *escaped_url;

		escaped_url = g_uri_escape_string(url, NULL, false);
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

	/* From here on, we only care about the first track, extract that
	 *
	 * Note: if you want to get information about the next track (needed
	 *   for continuous playback) extract the other track info here too.
	 */

	g_free(stream_url);
	track = lastfm_xmltag(response, "track");
	g_free(response);

	/* If there are no tracks in the tracklist, it's possible that the
	 * station doesn't have enough content.
	 */

	if (track == NULL)
		return false;

	stream_url = lastfm_xmltag(track, "location");
	if (stream_url == NULL) {
		g_free(track);
		return false;
	}

	/* now really open the last.fm radio stream */

	success = input_stream_open(is, stream_url);
	if (success) {
		/* instantiate our transparent wrapper plugin
		 * this is needed so that the backend knows what functions are
		 * properly available.
		 */

		struct lastfm_input *d = g_new0(struct lastfm_input, 1);
		d->wrap_plugin.name = "lastfm";
		d->wrap_plugin.open = lastfm_input_open;
		d->wrap_plugin.close = lastfm_input_close;
		d->wrap_plugin.read = lastfm_input_read_wrap;
		d->wrap_plugin.eof = lastfm_input_eof_wrap;
		d->wrap_plugin.tag = lastfm_input_tag;
		if (is->seekable)
			d->wrap_plugin.seek = lastfm_input_seek_wrap;
		if (is->plugin->buffer)
			d->wrap_plugin.buffer = lastfm_input_buffer_wrap;

		d->tag = lastfm_read_tag(track);
		d->plugin = is->plugin;
		d->data = is->data;

		/* give the backend our wrapper plugin */

		is->plugin = &d->wrap_plugin;
		is->data = d;
	}

	g_free(stream_url);
	g_free(track);

	return success;
}

const struct input_plugin lastfm_input_plugin = {
	.name = "lastfm",
	.init = lastfm_input_init,
	.finish = lastfm_input_finish,
	.open = lastfm_input_open,
	.close = lastfm_input_close,
	.tag  = lastfm_input_tag,
};
