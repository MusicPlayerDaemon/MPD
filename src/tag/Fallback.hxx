// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_TAG_FALLBACK_HXX
#define MPD_TAG_FALLBACK_HXX

#include "Type.hxx"

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

	if (type == TAG_TITLE_SORT)
		/* fall back to "Title" if no "TitleSort" was found */
		return f(TAG_TITLE);

	if (type == TAG_COMPOSERSORT)
		/* fall back to "Composer" if no "ComposerSort" was found */
		return f(TAG_COMPOSER);

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
