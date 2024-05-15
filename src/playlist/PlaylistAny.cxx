// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "LocateUri.hxx"
#include "PlaylistAny.hxx"
#include "PlaylistStream.hxx"
#include "PlaylistMapper.hxx"
#include "SongEnumerator.hxx"
#include "config.h"

std::unique_ptr<SongEnumerator>
playlist_open_any(const LocatedUri &located_uri,
#ifdef ENABLE_DATABASE
		  Storage *storage,
#endif
		  Mutex &mutex)
{
	switch (located_uri.type) {
	case LocatedUri::Type::ABSOLUTE:
		return playlist_open_remote(located_uri.canonical_uri, mutex);

	case LocatedUri::Type::PATH:
		return playlist_open_path(located_uri.path, mutex);

	case LocatedUri::Type::RELATIVE:
		return playlist_mapper_open(located_uri.canonical_uri,
#ifdef ENABLE_DATABASE
				       storage,
#endif
				       mutex);
	}

	gcc_unreachable();
}
