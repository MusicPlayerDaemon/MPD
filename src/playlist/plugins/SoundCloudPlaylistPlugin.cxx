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
#include "SoundCloudPlaylistPlugin.hxx"
#include "../PlaylistPlugin.hxx"
#include "../MemorySongEnumerator.hxx"
#include "config/ConfigData.hxx"
#include "input/InputStream.hxx"
#include "tag/TagBuilder.hxx"
#include "util/StringUtil.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <glib.h>
#include <yajl/yajl_parse.h>

#include <string>

#include <string.h>

static struct {
	std::string apikey;
} soundcloud_config;

static constexpr Domain soundcloud_domain("soundcloud");

static bool
soundcloud_init(const config_param &param)
{
	// APIKEY for MPD application, registered under DarkFox' account.
	soundcloud_config.apikey = param.GetBlockValue("apikey", "a25e51780f7f86af0afa91f241d091f8");
	if (soundcloud_config.apikey.empty()) {
		LogDebug(soundcloud_domain,
			 "disabling the soundcloud playlist plugin "
			 "because API key is not set");
		return false;
	}

	return true;
}

/**
 * Construct a full soundcloud resolver URL from the given fragment.
 * @param uri uri of a soundcloud page (or just the path)
 * @return Constructed URL. Must be freed with g_free.
 */
static char *
soundcloud_resolve(const char* uri)
{
	char *u, *ru;

	if (StringStartsWith(uri, "https://")) {
		u = g_strdup(uri);
	} else if (StringStartsWith(uri, "soundcloud.com")) {
		u = g_strconcat("https://", uri, nullptr);
	} else {
		/* assume it's just a path on soundcloud.com */
		u = g_strconcat("https://soundcloud.com/", uri, nullptr);
	}

	ru = g_strconcat("https://api.soundcloud.com/resolve.json?url=",
			 u, "&client_id=",
			 soundcloud_config.apikey.c_str(), nullptr);
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
	nullptr,
};

struct parse_data {
	int key;
	char* stream_url;
	long duration;
	char* title;
	int got_url; /* nesting level of last stream_url */

	std::forward_list<DetachedSong> songs;
};

static int
handle_integer(void *ctx,
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

static int
handle_string(void *ctx, const unsigned char* stringval,
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
		g_free(data->title);
		data->title = g_strndup(s, stringlen);
		break;
	case Stream_URL:
		g_free(data->stream_url);
		data->stream_url = g_strndup(s, stringlen);
		data->got_url = 1;
		break;
	default:
		break;
	}

	return 1;
}

static int
handle_mapkey(void *ctx, const unsigned char* stringval,
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
		if (memcmp((const char *)stringval, key_str[i], stringlen) == 0) {
			data->key = i;
			break;
		}
	}

	return 1;
}

static int
handle_start_map(void *ctx)
{
	struct parse_data *data = (struct parse_data *) ctx;

	if (data->got_url > 0)
		data->got_url++;

	return 1;
}

static int
handle_end_map(void *ctx)
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

	char *u = g_strconcat(data->stream_url, "?client_id=",
			      soundcloud_config.apikey.c_str(), nullptr);

	TagBuilder tag;
	tag.SetDuration(SignedSongTime::FromMS(data->duration));
	if (data->title != nullptr)
		tag.AddItem(TAG_NAME, data->title);

	data->songs.emplace_front(u, tag.Commit());
	g_free(u);

	return 1;
}

static yajl_callbacks parse_callbacks = {
	nullptr,
	nullptr,
	handle_integer,
	nullptr,
	nullptr,
	handle_string,
	handle_start_map,
	handle_mapkey,
	handle_end_map,
	nullptr,
	nullptr,
};

/**
 * Read JSON data and parse it using the given YAJL parser.
 * @param url URL of the JSON data.
 * @param hand YAJL parser handle.
 * @return -1 on error, 0 on success.
 */
static int
soundcloud_parse_json(const char *url, yajl_handle hand,
		      Mutex &mutex, Cond &cond)
{
	Error error;
	InputStream *input_stream = InputStream::OpenReady(url, mutex, cond,
							   error);
	if (input_stream == nullptr) {
		if (error.IsDefined())
			LogError(error);
		return -1;
	}

	mutex.lock();

	yajl_status stat;
	int done = 0;

	while (!done) {
		char buffer[4096];
		unsigned char *ubuffer = (unsigned char *)buffer;
		const size_t nbytes =
			input_stream->Read(buffer, sizeof(buffer), error);
		if (nbytes == 0) {
			if (error.IsDefined())
				LogError(error);

			if (input_stream->IsEOF()) {
				done = true;
			} else {
				mutex.unlock();
				delete input_stream;
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
			LogError(soundcloud_domain, (const char *)str);
			yajl_free_error(hand, str);
			break;
		}
	}

	mutex.unlock();
	delete input_stream;

	return 0;
}

/**
 * Parse a soundcloud:// URL and create a playlist.
 * @param uri A soundcloud URL. Accepted forms:
 *	soundcloud://track/<track-id>
 *	soundcloud://playlist/<playlist-id>
 *	soundcloud://url/<url or path of soundcloud page>
 */
static SongEnumerator *
soundcloud_open_uri(const char *uri, Mutex &mutex, Cond &cond)
{
	assert(memcmp(uri, "soundcloud://", 13) == 0);
	uri += 13;

	char *u = nullptr;
	if (memcmp(uri, "track/", 6) == 0) {
		const char *rest = uri + 6;
		u = g_strconcat("https://api.soundcloud.com/tracks/",
				rest, ".json?client_id=",
				soundcloud_config.apikey.c_str(), nullptr);
	} else if (memcmp(uri, "playlist/", 9) == 0) {
		const char *rest = uri + 9;
		u = g_strconcat("https://api.soundcloud.com/playlists/",
				rest, ".json?client_id=",
				soundcloud_config.apikey.c_str(), nullptr);
	} else if (memcmp(uri, "user/", 5) == 0) {
		const char *rest = uri + 5;
		u = g_strconcat("https://api.soundcloud.com/users/",
				rest, "/tracks.json?client_id=",
				soundcloud_config.apikey.c_str(), nullptr);
	} else if (memcmp(uri, "search/", 7) == 0) {
		const char *rest = uri + 7;
		u = g_strconcat("https://api.soundcloud.com/tracks.json?q=",
				rest, "&client_id=",
				soundcloud_config.apikey.c_str(), nullptr);
	} else if (memcmp(uri, "url/", 4) == 0) {
		const char *rest = uri + 4;
		/* Translate to soundcloud resolver call. libcurl will automatically
		   follow the redirect to the right resource. */
		u = soundcloud_resolve(rest);
	}

	if (u == nullptr) {
		LogWarning(soundcloud_domain, "unknown soundcloud URI");
		return nullptr;
	}

	struct parse_data data;
	data.got_url = 0;
	data.title = nullptr;
	data.stream_url = nullptr;
#ifdef HAVE_YAJL1
	yajl_handle hand = yajl_alloc(&parse_callbacks, nullptr, nullptr,
				      &data);
#else
	yajl_handle hand = yajl_alloc(&parse_callbacks, nullptr, &data);
#endif

	int ret = soundcloud_parse_json(u, hand, mutex, cond);

	g_free(u);
	yajl_free(hand);
	g_free(data.title);
	g_free(data.stream_url);

	if (ret == -1)
		return nullptr;

	data.songs.reverse();
	return new MemorySongEnumerator(std::move(data.songs));
}

static const char *const soundcloud_schemes[] = {
	"soundcloud",
	nullptr
};

const struct playlist_plugin soundcloud_playlist_plugin = {
	"soundcloud",

	soundcloud_init,
	nullptr,
	soundcloud_open_uri,
	nullptr,

	soundcloud_schemes,
	nullptr,
	nullptr,
};


