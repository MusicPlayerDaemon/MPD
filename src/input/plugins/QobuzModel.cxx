/*
 * Copyright 2003-2018 The Music Player Daemon Project
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
#include "QobuzModel.hxx"

#include "external/jaijson/Deserializer.hxx"

static bool
deserialize(const jaijson::Value &d, Performer &m)
{
	return deserialize(d, "name", m.name);
}

static bool
deserialize(const jaijson::Value &d, Genre &m)
{
	return deserialize(d, "name", m.name);
}

static bool
deserialize(const jaijson::Value &d, Image &m)
{
	//deserialize(d, "thumbnail", m.thumbnail);
	//deserialize(d, "back", m.back);
	//deserialize(d, "small", m.small);
	deserialize(d, "large", m.large);
	if (m.large.empty()) {
		deserialize(d, "small", m.large);
	}
	if (m.large.empty()) {
		deserialize(d, "back", m.large);
	}
	if (m.large.empty()) {
		deserialize(d, "thumbnail", m.large);
	}

	return true;
}

static bool
deserialize(const jaijson::Value &d, ArtistSimple &m)
{
	return deserialize(d, "name", m.name);
}

static bool
deserialize(const jaijson::Value &d, TrackSimple &m)
{
	deserialize(d, "id", m.id);
	deserialize(d, "title", m.title);
	deserialize(d, "duration", m.duration);
	deserialize(d, "performer", m.performer);

	return m.id > 0;
}

static bool
deserialize(const jaijson::Value &d, AlbumSimple &m)
{
	deserialize(d, "id", m.id);
	deserialize(d, "title", m.title);
	deserialize(d, "artist", m.artist);
	deserialize(d, "genre", m.genre);
	deserialize(d, "image", m.image);
	deserialize(d, "tracks_count", m.tracks_count);

	return !m.id.empty();
}

static bool
deserialize(const jaijson::Value &d, PlaylistSimple &m)
{
	deserialize(d, "id", m.id);
	deserialize(d, "name", m.name);
	deserialize(d, "tracks_count", m.tracks_count);

	return m.id > 0;
}

template<class T>
static bool
deserialize(const jaijson::Value &d, PageItems<T> &m)
{
	deserialize(d, "offset", m.offset);
	deserialize(d, "limit", m.limit);
	deserialize(d, "total", m.total);
	deserialize(d, "items", m.items);

	return true;
}

bool
deserialize(const jaijson::Value &d, Album &m)
{
	deserialize(d, static_cast<AlbumSimple&>(m));
	deserialize(d, "tracks", m.tracks);

	return !m.id.empty();
}

bool
deserialize(const jaijson::Value &d, Track &m)
{
	deserialize(d, static_cast<TrackSimple&>(m));
	deserialize(d, "album", m.album);
	deserialize(d, "artist", m.artist);

	return m.id > 0;
}

bool
deserialize(const jaijson::Value &d, Playlist &m)
{
	deserialize(d, static_cast<PlaylistSimple&>(m));
	deserialize(d, "tracks", m.tracks);

	return m.id > 0;
}

bool
deserialize(const jaijson::Value &d, StreamTrack &m)
{
	deserialize(d, "track_id", m.track_id);
	deserialize(d, "duration", m.duration);
	deserialize(d, "url", m.url);
	deserialize(d, "format_id", m.format_id);
	deserialize(d, "mime_type", m.mime_type);
	deserialize(d, "sampling_rate", m.sampling_rate);
	deserialize(d, "bit_depth", m.bit_depth);

	return !m.url.empty();
}
