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

#ifndef MPD_INPUT_STREAM_HANDLER_HXX
#define MPD_INPUT_STREAM_HANDLER_HXX

/**
 * An interface which gets receives events from an #InputStream.  Its
 * methods will be called from within an arbitrary thread and must not
 * block.
 *
 * A reference to an instance is passed to the #InputStream, but it
 * remains owned by the caller.
 */
class InputStreamHandler {
public:
	/**
	 * Called when InputStream::IsReady() becomes true.
	 *
	 * Before querying metadata from the #InputStream,
	 * InputStream::Update() must be called.
	 *
	 * Caller locks InputStream::mutex.
	 */
	virtual void OnInputStreamReady() noexcept = 0;

	/**
	 * Called when InputStream::IsAvailable() becomes true.
	 *
	 * Caller locks InputStream::mutex.
	 */
	virtual void OnInputStreamAvailable() noexcept = 0;
};

#endif
