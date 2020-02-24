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

#ifndef QOBUZ_MODEL_HXX
#define QOBUZ_MODEL_HXX

#include "check.h"

#include "external/jaijson/jaijson.hxx"

#include <string>
#include <list>

struct Performer {
	std::string name;
};

struct Genre {
	std::string name;
};

struct Image {
	//std::string thumbnail;
	//std::string back;
	//std::string small;
	std::string large;
};

struct ArtistSimple {
	std::string name;
};

struct TrackSimple {
	int64_t id = -1;
	std::string title;
	int duration;
	Performer performer;
};

struct AlbumSimple {
	std::string id;
	std::string title;
	ArtistSimple artist;
	Genre genre;
	Image image;
	int tracks_count = -1;
};

struct PlaylistSimple {
	int64_t id = -1;
	std::string name;
	int tracks_count = -1;
};

template<class T>
struct PageItems {
	int offset = 0;
	int limit = 1000;
	int total = 0;
	std::list<T> items;
};

struct Album: AlbumSimple {
	PageItems<TrackSimple> tracks;
};

struct Track: TrackSimple {
	AlbumSimple album;
	ArtistSimple artist;
};

struct Playlist: PlaylistSimple {
	PageItems<Track> tracks;
};

struct StreamTrack {
	int64_t track_id = 0;
	int64_t duration = 0;
	std::string url;
	int format_id = 5;
	std::string mime_type;
	double sampling_rate = 0;
	int bit_depth = 0;
};

bool deserialize(const jaijson::Value &d, Track &m);
bool deserialize(const jaijson::Value &d, Album &m);
bool deserialize(const jaijson::Value &d, Playlist &m);
bool deserialize(const jaijson::Value &d, StreamTrack &m);

#endif
