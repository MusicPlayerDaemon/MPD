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
#include "PlsPlaylistPlugin.hxx"
#include "MemoryPlaylistProvider.hxx"
#include "InputLegacy.hxx"
#include "Song.hxx"
#include "Tag.hxx"
#include "util/Error.hxx"

#include <glib.h>

#include <string>

static void
pls_parser(GKeyFile *keyfile, std::forward_list<SongPointer> &songs)
{
	gchar *key;
	gchar *value;
	int length;
	GError *error = NULL;
	int num_entries = g_key_file_get_integer(keyfile, "playlist",
						 "NumberOfEntries", &error);
	if (error) {
		g_debug("Invalid PLS file: '%s'", error->message);
		g_error_free(error);
		error = NULL;

		/* Hack to work around shoutcast failure to comform to spec */
		num_entries = g_key_file_get_integer(keyfile, "playlist",
						     "numberofentries", &error);
		if (error) {
			g_error_free(error);
			error = NULL;
		}
	}

	while (num_entries > 0) {
		Song *song;
		key = g_strdup_printf("File%i", num_entries);
		value = g_key_file_get_string(keyfile, "playlist", key,
					      &error);
		if(error) {
			g_debug("Invalid PLS entry %s: '%s'",key, error->message);
			g_error_free(error);
			g_free(key);
			return;
		}
		g_free(key);

		song = Song::NewRemote(value);
		g_free(value);

		key = g_strdup_printf("Title%i", num_entries);
		value = g_key_file_get_string(keyfile, "playlist", key,
					      &error);
		g_free(key);
		if(error == NULL && value){
			if (song->tag == NULL)
				song->tag = new Tag();
			song->tag->AddItem(TAG_TITLE, value);
		}
		/* Ignore errors? Most likely value not present */
		if(error) g_error_free(error);
		error = NULL;
		g_free(value);

		key = g_strdup_printf("Length%i", num_entries);
		length = g_key_file_get_integer(keyfile, "playlist", key,
						&error);
		g_free(key);
		if(error == NULL && length > 0){
			if (song->tag == NULL)
				song->tag = new Tag();
			song->tag->time = length;
		}
		/* Ignore errors? Most likely value not present */
		if(error) g_error_free(error);
		error = NULL;

		songs.emplace_front(song);
		num_entries--;
	}

}

static struct playlist_provider *
pls_open_stream(struct input_stream *is)
{
	GError *error = NULL;
	Error error2;
	size_t nbytes;
	char buffer[1024];
	bool success;
	GKeyFile *keyfile;

	std::string kf_data;

	do {
		nbytes = input_stream_lock_read(is, buffer, sizeof(buffer),
						error2);
		if (nbytes == 0) {
			if (error2.IsDefined()) {
				g_warning("%s", error2.GetMessage());
				return NULL;
			}

			break;
		}

		kf_data.append(buffer, nbytes);
		/* Limit to 64k */
	} while (kf_data.length() < 65536);

	if (kf_data.empty()) {
		g_warning("KeyFile parser failed: No Data");
		return NULL;
	}

	keyfile = g_key_file_new();
	success = g_key_file_load_from_data(keyfile,
					    kf_data.data(), kf_data.length(),
					    G_KEY_FILE_NONE, &error);

	if (!success) {
		g_warning("KeyFile parser failed: %s", error->message);
		g_error_free(error);
		g_key_file_free(keyfile);
		return NULL;
	}

	std::forward_list<SongPointer> songs;
	pls_parser(keyfile, songs);
	g_key_file_free(keyfile);

	songs.reverse();
	return new MemoryPlaylistProvider(std::move(songs));
}

static const char *const pls_suffixes[] = {
	"pls",
	NULL
};

static const char *const pls_mime_types[] = {
	"audio/x-scpls",
	NULL
};

const struct playlist_plugin pls_playlist_plugin = {
	"pls",

	nullptr,
	nullptr,
	nullptr,
	pls_open_stream,
	nullptr,
	nullptr,

	nullptr,
	pls_suffixes,
	pls_mime_types,
};
