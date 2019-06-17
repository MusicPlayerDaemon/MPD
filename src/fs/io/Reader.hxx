/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#ifndef MPD_READER_HXX
#define MPD_READER_HXX

#include "util/Compiler.h"

#include <stddef.h>

/**
 * An interface that can read bytes from a stream until the stream
 * ends.
 *
 * This interface is simpler and less cumbersome to use than
 * #InputStream.
 */
class Reader {
public:
	Reader() = default;
	Reader(const Reader &) = delete;

	/**
	 * Read data from the stream.
	 *
	 * @return the number of bytes read into the given buffer or 0
	 * on end-of-stream
	 */
	gcc_nonnull_all
	virtual size_t Read(void *data, size_t size) = 0;
};

#endif
