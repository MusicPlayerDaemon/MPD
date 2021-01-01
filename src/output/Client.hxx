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

#ifndef MPD_OUTPUT_CLIENT_HXX
#define MPD_OUTPUT_CLIENT_HXX

/**
 * An interface between the #AudioOutput and the #Player.
 */
class AudioOutputClient {
public:
	/**
	 * Notify the client that we have consumed a few chunks.  This
	 * is called from within the output thread.  The client may
	 * perform actions to refill the #MusicPipe.
	 */
	virtual void ChunksConsumed() = 0;

	/**
	 * The #AudioOutput has modified the "enabled" flag, and the
	 * client shall make the #AudioOutput apply this new setting.
	 * This is called from any thread, one which can't send an
	 * AudioOutput::Command to the output thread; only the client
	 * can do that safely.
	 */
	virtual void ApplyEnabled() = 0;
};

#endif
