/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "playlist/soundcloud_playlist_plugin.h"
#include "conf.h"
#include "input_stream.h"
#include "playlist_plugin.h"
#include "song.h"
#include "tag.h"

#include <glib.h>
#include <yajl/yajl_parse.h>

#include <string.h>

struct soundcloud_playlist {
	struct playlist_provider base;

	GSList *songs;
};

static struct {
	char *apikey;
} soundcloud_config;

static bool
soundcloud_init(const struct config_param *param)
{
	soundcloud_config.apikey =
		config_dup_block_string(param, "apikey", NULL);
	if (soundcloud_config.apikey == NULL) {
		g_debug("disabling the soundcloud playlist plugin "
			"because API key is not set");
		return false;
	}

	return true;
}

static void
soundcloud_finish(void)
{
	g_free(soundcloud_config.apikey);
}

/**
 * Construct a full soundcloud resolver URL from the given fragment.
 * @param uri uri of a soundcloud page (or just the path)
 * @return Constructed URL. Must be freed with g_free.
 */
static char *
soundcloud_resolve(const char* uri) {
	char *u, *ru;

	if (g_str_has_prefix(uri, "http://")) {
		u = g_strdup(uri);
	} else if (g_str_has_prefix(uri, "soundcloud.com")) {
		u = g_strconcat("http://", uri, NULL);
	} else {
		/* assume it's just a path on soundcloud.com */
		u = g_strconcat("http://soundcloud.com/", uri, NULL);
	}

	ru = g_strconcat("http://api.soundcloud.com/resolve.json?url=",
			u, "&client_id=", soundcloud_config.apikey, NULL);
	g_free(u);

	return ru;
}

/* YAJL parser for track data from both /tracks/ and /playlists/ JSON */

enum key {
	Duration,
	Title,
	Stream_URL,
	Other,
};

const char* key_str[] = {
	"duration",
	"title",
	"stream_url",
	NULL,
};

struct parse_data {
	int key;
	char* stream_url;
	long duration;
	char* title;
	int got_url; /* nesting level of last stream_url */
	GSList* songs;
};

static int handle_integer(void *ctx,
			  long
#ifndef HAVE_YAJL1
			  long
#endif
			  intval)
{
	struct parse_data *data = (struct parse_data *) ctx;

	switch (data->key) {
	case Duration:
		data->duration = intval;
		break;
	default:
		break;
	}

	return 1;
}

static int handle_string(void *ctx, const unsigned char* stringval,
#ifdef HAVE_YAJL1
			 unsigned int
#else
			 size_t
#endif
			 stringlen)
{
	struct parse_data *data = (struct parse_data *) ctx;
	const char *s = (const char *) stringval;

	switch (data->key) {
	case Title:
		if (data->title != NULL)
			g_free(data->title);
		data->title = g_strndup(s, stringlen);
		break;
	case Stream_URL:
		if (data->stream_url != NULL)
			g_free(data->stream_url);
		data->stream_url = g_strndup(s, stringlen);
		data->got_url = 1;
		break;
	default:
		break;
	}

	return 1;
}

static int handle_mapkey(void *ctx, const unsigned char* stringval,
#ifdef HAVE_YAJL1
			 unsigned int
#else
			 size_t
#endif
			 stringlen)
{
	struct parse_data *data = (struct parse_data *) ctx;

	int i;
	data->key = Other;

	for (i = 0; i < Other; ++i) {
		if (strncmp((const char *)stringval, key_str[i], stringlen) == 0) {
			data->key = i;
			break;
		}
	}

	return 1;
}

static int handle_start_map(void *ctx)
{
	struct parse_data *data = (struct parse_data *) ctx;

	if (data->got_url > 0)
		data->got_url++;

	return 1;
}

static int handle_end_map(void *ctx)
{
	struct parse_data *data = (struct parse_data *) ctx;

	if (data->got_url > 1) {
		data->got_url--;
		return 1;
	}

	if (data->got_url == 0)
		return 1;

	/* got_url == 1, track finished, make it into a song */
	data->got_url = 0;

	struct song *s;
	struct tag *t;
	char *u;

	u = g_strconcat(data->stream_url, "?client_id=", soundcloud_config.apikey, NULL);
	s = song_remote_new(u);
	g_free(u);
	t = tag_new();
	t->time = data->duration / 1000;
	if (data->title != NULL)
		tag_add_item(t, TAG_NAME, data->title);
	s->tag = t;

	data->songs = g_slist_prepend(data->songs, s);

	return 1;
}

static yajl_callbacks parse_callbacks = {
	NULL,
	NULL,
	handle_integer,
	NULL,
	NULL,
	handle_string,
	handle_start_map,
	handle_mapkey,
	handle_end_map,
	NULL,
	NULL,
};

/**
 * Read JSON data and parse it using the given YAJL parser.
 * @param url URL of the JSON data.
 * @param hand YAJL parser handle.
 * @return -1 on error, 0 on success.
 */
static int
soundcloud_parse_json(const char *url, yajl_handle hand, GMutex* mutex, GCond* cond)
{
	struct input_stream *input_stream;
	GError *error = NULL;
	char buffer[4096];
	unsigned char *ubuffer = (unsigned char *)buffer;
	size_t nbytes;

	input_stream = input_stream_open(url, mutex, cond, &error);
	if (input_stream == NULL) {
		if (error != NULL) {
			g_warning("%s", error->message);
			g_error_free(error);
		}
		return -1;
	}

	g_mutex_lock(mutex);
	input_stream_wait_ready(input_stream);

	yajl_status stat;
	int done = 0;

	while (!done) {
		nbytes = input_stream_read(input_stream, buffer, sizeof(buffer), &error);
		if (nbytes == 0) {
			if (error != NULL) {
				g_warning("%s", error->message);
				g_error_free(error);
			}
			if (input_stream_eof(input_stream)) {
				done = true;
			} else {
				g_mutex_unlock(mutex);
				input_stream_close(input_stream);
				return -1;
			}
		}

		if (done) {
#ifdef HAVE_YAJL1
			stat = yajl_parse_complete(hand);
#else
			stat = yajl_complete_parse(hand);
#endif
		} else
			stat = yajl_parse(hand, ubuffer, nbytes);

		if (stat != yajl_status_ok
#ifdef HAVE_YAJL1
		    && stat != yajl_status_insufficient_data
#endif
		    )
		{
			unsigned char *str = yajl_get_error(hand, 1, ubuffer, nbytes);
			g_warning("%s", str);
			yajl_free_error(hand, str);
			break;
		}
	}

	g_mutex_unlock(mutex);
	input_stream_close(input_stream);

	return 0;
}

/**
 * Parse a soundcloud:// URL and create a playlist.
 * @param uri A soundcloud URL. Accepted forms:
 *	soundcloud://track/<track-id>
 *	soundcloud://playlist/<playlist-id>
 *	soundcloud://url/<url or path of soundcloud page>
 */

static struct playlist_provider *
soundcloud_open_uri(const char *uri, GMutex *mutex, GCond *cond)
{
	struct soundcloud_playlist *playlist = NULL;

	char *s, *p;
	char *scheme, *arg, *rest;
	s = g_strdup(uri);
	scheme = s;
	for (p = s; *p; p++) {
		if (*p == ':' && *(p+1) == '/' && *(p+2) == '/') {
			*p = 0;
			p += 3;
			break;
		}
	}
	arg = p;
	for (; *p; p++) {
		if (*p == '/') {
			*p = 0;
			p++;
			break;
		}
	}
	rest = p;

	if (strcmp(scheme, "soundcloud") != 0) {
		g_warning("incompatible scheme for soundcloud plugin: %s", scheme);
		g_free(s);
		return NULL;
	}

	char *u = NULL;
	if (strcmp(arg, "track") == 0) {
		u = g_strconcat("http://api.soundcloud.com/tracks/",
			rest, ".json?client_id=", soundcloud_config.apikey, NULL);
	} else if (strcmp(arg, "playlist") == 0) {
		u = g_strconcat("http://api.soundcloud.com/playlists/",
			rest, ".json?client_id=", soundcloud_config.apikey, NULL);
	} else if (strcmp(arg, "url") == 0) {
		/* Translate to soundcloud resolver call. libcurl will automatically
		   follow the redirect to the right resource. */
		u = soundcloud_resolve(rest);
	}
	g_free(s);

	if (u == NULL) {
		g_warning("unknown soundcloud URI");
		return NULL;
	}

	yajl_handle hand;
	struct parse_data data;

	data.got_url = 0;
	data.songs = NULL;
	data.title = NULL;
	data.stream_url = NULL;
#ifdef HAVE_YAJL1
	hand = yajl_alloc(&parse_callbacks, NULL, NULL, (void *) &data);
#else
	hand = yajl_alloc(&parse_callbacks, NULL, (void *) &data);
#endif

	int ret = soundcloud_parse_json(u, hand, mutex, cond);

	g_free(u);
	yajl_free(hand);
	if (data.title != NULL)
		g_free(data.title);
	if (data.stream_url != NULL)
		g_free(data.stream_url);

	if (ret == -1)
		return NULL;

	playlist = g_new(struct soundcloud_playlist, 1);
	playlist_provider_init(&playlist->base, &soundcloud_playlist_plugin);
	playlist->songs = g_slist_reverse(data.songs);

	return &playlist->base;
}

static void
soundcloud_close(struct playlist_provider *_playlist)
{
	struct soundcloud_playlist *playlist = (struct soundcloud_playlist *)_playlist;

	g_free(playlist);
}


static struct song *
soundcloud_read(struct playlist_provider *_playlist)
{
	struct soundcloud_playlist *playlist = (struct soundcloud_playlist *)_playlist;

	if (playlist->songs == NULL)
		return NULL;

	struct song* s;
	s = (struct song *)playlist->songs->data;
	playlist->songs = g_slist_remove(playlist->songs, s);
	return s;
}

static const char *const soundcloud_schemes[] = {
	"soundcloud",
	NULL
};

const struct playlist_plugin soundcloud_playlist_plugin = {
	.name = "soundcloud",

	.init = soundcloud_init,
	.finish = soundcloud_finish,
	.open_uri = soundcloud_open_uri,
	.close = soundcloud_close,
	.read = soundcloud_read,

	.schemes = soundcloud_schemes,
};


