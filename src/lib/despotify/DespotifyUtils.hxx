/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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

#ifndef MPD_DESPOTIFY_UTILS_HXX
#define MPD_DESPOTIFY_UTILS_HXX

struct Tag;
struct despotify_session;
struct ds_track;

extern const class Domain despotify_domain;

/**
 * Return the current despotify session.
 *
 * If the session isn't initialized, this function will initialize
 * it and connect to Spotify.
 *
 * @return a pointer to the despotify session, or nullptr if it can't
 *         be initialized (e.g., if the configuration isn't supplied)
 */
struct despotify_session *
mpd_despotify_get_session();

/**
 * Create a MPD tags structure from a spotify track
 *
 * @param track the track to convert
 *
 * @return filled in #Tag structure
 */
Tag
mpd_despotify_tag_from_track(const ds_track &track);

/**
 * Register a despotify callback.
 *
 * Despotify calls this e.g., when a track ends.
 *
 * @param cb the callback
 * @param cb_data the data to pass to the callback
 *
 * @return true if the callback could be registered
 */
bool
mpd_despotify_register_callback(void (*cb)(struct despotify_session *, int,
					   void *, void *),
				void *cb_data);

/**
 * Unregister a despotify callback.
 *
 * @param cb the callback to unregister.
 */
void
mpd_despotify_unregister_callback(void (*cb)(struct despotify_session *, int,
					     void *, void *));

#endif

