/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#ifndef MPD_TAG_FALLBACK_HXX
#define MPD_TAG_FALLBACK_HXX

#include <utility>

/**
 * Invoke the given function for all fallback tags of the given
 * #TagType, until the function returns true (or until there are no
 * more fallback tags).
 */
template<typename F>
bool
ApplyTagFallback(TagType type, F &&f) noexcept
{
	if (type == TAG_ALBUM_ARTIST_SORT) {
		/* fall back to "AlbumArtist", "ArtistSort" and
		   "Artist" if no "AlbumArtistSort" was found */
		if (f(TAG_ALBUM_ARTIST))
			return true;

		return ApplyTagFallback(TAG_ARTIST_SORT, std::forward<F>(f));
	}

	if (type == TAG_ALBUM_ARTIST || type == TAG_ARTIST_SORT)
		/* fall back to "Artist" if no
		   "AlbumArtist"/"ArtistSort" was found */
		return f(TAG_ARTIST);

	if (type == TAG_ALBUM_SORT)
		/* fall back to "Album" if no "AlbumSort" was found */
		return f(TAG_ALBUM);

	return false;
}

/**
 * Invoke the given function for the given #TagType and all of its
 * fallback tags, until the function returns true (or until there are
 * no more fallback tags).
 */
template<typename F>
bool
ApplyTagWithFallback(TagType type, F &&f) noexcept
{
	return f(type) || ApplyTagFallback(type, std::forward<F>(f));
}

#endif
