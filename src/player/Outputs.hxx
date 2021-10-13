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

#ifndef MPD_PLAYER_OUTPUT_INTERFACE_HXX
#define MPD_PLAYER_OUTPUT_INTERFACE_HXX

#include "MusicChunkPtr.hxx"
#include "Chrono.hxx"

struct AudioFormat;
struct MusicChunk;

/**
 * An interface for the player thread to control all outputs.  This
 * interface is implemented only by #MultipleOutputs, and exists only
 * to decouple the player code from the output code, to be able to
 * unit-test the player code.
 */
class PlayerOutputs {
public:
	/**
	 * Checks the "enabled" flag of all audio outputs, and if one has
	 * changed, commit the change.
	 *
	 * Throws on error.
	 */
	virtual void EnableDisable() = 0;

	/**
	 * Opens all audio outputs which are not disabled.
	 *
	 * Throws on error.
	 *
	 * @param audio_format the preferred audio format
	 */
	virtual void Open(const AudioFormat audio_format) = 0;

	/**
	 * Closes all audio outputs.
	 */
	virtual void Close() noexcept = 0;

	/**
	 * Closes all audio outputs.  Outputs with the "always_on"
	 * flag are put into pause mode.
	 */
	virtual void Release() noexcept = 0;

	/**
	 * Enqueue a #MusicChunk object for playing, i.e. pushes it to a
	 * #MusicPipe.
	 *
	 * Throws on error (all closed then).
	 *
	 * @param chunk the #MusicChunk object to be played
	 */
	virtual void Play(MusicChunkPtr chunk) = 0;

	/**
	 * Checks if the output devices have drained their music pipe, and
	 * returns the consumed music chunks to the #music_buffer.
	 *
	 * @return the number of chunks to play left in the #MusicPipe
	 */
	virtual unsigned CheckPipe() noexcept = 0;

	/**
	 * Puts all audio outputs into pause mode.  Most implementations will
	 * simply close it then.
	 */
	virtual void Pause() noexcept = 0;

	/**
	 * Drain all audio outputs.
	 */
	virtual void Drain() noexcept = 0;

	/**
	 * Try to cancel data which may still be in the device's buffers.
	 */
	virtual void Cancel() noexcept = 0;

	/**
	 * Indicate that a new song will begin now.
	 */
	virtual void SongBorder() noexcept = 0;

	/**
	 * Returns the "elapsed_time" stamp of the most recently finished
	 * chunk.  A negative value is returned when no chunk has been
	 * finished yet.
	 */
	[[gnu::pure]]
	virtual SignedSongTime GetElapsedTime() const noexcept = 0;
};

#endif
