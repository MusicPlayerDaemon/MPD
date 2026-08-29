// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/**
 * Mock implementation of playlist_check_translate_song for testing.
 *
 * This minimal mock allows songs to be loaded into the queue during tests
 * without requiring a real database or file system. It simply returns true
 * for all songs, bypassing the normal validation that would fail in a test
 * environment.
 */

#include "playlist/PlaylistSong.hxx"
#include "song/DetachedSong.hxx"

/**
 * Mock implementation that always allows songs to load.
 *
 * In production, this function validates that:
 * - Songs exist in the database or filesystem
 * - URIs are properly formatted
 * - Files are accessible
 *
 * For testing, we bypass all validation and allow any song to load.
 * This enables testing of state file read/write logic without needing
 * a real music database.
 *
 * @param song The song to validate (modified in place if needed)
 * @param base_uri Base URI for resolving relative paths (unused in mock)
 * @param loader Song loader for database access (unused in mock)
 * @return Always returns true to allow the song to be added to the queue
 */
bool
playlist_check_translate_song(DetachedSong &song [[maybe_unused]],
                              [[maybe_unused]] std::string_view base_uri,
                              [[maybe_unused]] const SongLoader &loader) noexcept
{
	// Always return true - allow all songs to load in tests
	return true;
}
