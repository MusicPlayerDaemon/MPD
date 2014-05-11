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

#ifndef MPD_PLAYLIST_REGISTRY_HXX
#define MPD_PLAYLIST_REGISTRY_HXX

class Mutex;
class Cond;
class SongEnumerator;
class InputStream;

extern const struct playlist_plugin *const playlist_plugins[];

#define playlist_plugins_for_each(plugin) \
	for (const struct playlist_plugin *plugin, \
		*const*playlist_plugin_iterator = &playlist_plugins[0]; \
		(plugin = *playlist_plugin_iterator) != nullptr; \
		++playlist_plugin_iterator)

/**
 * Initializes all playlist plugins.
 */
void
playlist_list_global_init(void);

/**
 * Deinitializes all playlist plugins.
 */
void
playlist_list_global_finish(void);

/**
 * Opens a playlist by its URI.
 */
SongEnumerator *
playlist_list_open_uri(const char *uri, Mutex &mutex, Cond &cond);

SongEnumerator *
playlist_list_open_stream_suffix(InputStream &is, const char *suffix);

/**
 * Opens a playlist from an input stream.
 *
 * @param is an #input_stream object which is open and ready
 * @param uri optional URI which was used to open the stream; may be
 * used to select the appropriate playlist plugin
 */
SongEnumerator *
playlist_list_open_stream(InputStream &is, const char *uri);

/**
 * Determines if there is a playlist plugin which can handle the
 * specified file name suffix.
 */
bool
playlist_suffix_supported(const char *suffix);

#endif
