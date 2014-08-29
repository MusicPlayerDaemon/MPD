/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#ifndef MPD_GZIP_OUTPUT_STREAM_HXX
#define MPD_GZIP_OUTPUT_STREAM_HXX

#include "check.h"
#include "OutputStream.hxx"
#include "Compiler.h"

#include <assert.h>
#include <zlib.h>

class Error;
class Domain;

/**
 * A filter that compresses data written to it using zlib, forwarding
 * compressed data in the "gzip" format.
 *
 * Don't forget to call Flush() before destructing this object.
 */
class GzipOutputStream final : public OutputStream {
	OutputStream &next;

	z_stream z;

public:
	/**
	 * Construct the filter.  Call IsDefined() to check whether
	 * the constructor has succeeded.  If not, #error will hold
	 * information about the failure.
	 */
	GzipOutputStream(OutputStream &_next, Error &error);
	~GzipOutputStream();

	/**
	 * Check whether the constructor has succeeded.
	 */
	bool IsDefined() const {
		return z.opaque == nullptr;
	}

	/**
	 * Finish the file and write all data remaining in zlib's
	 * output buffer.
	 */
	bool Flush(Error &error);

	/* virtual methods from class OutputStream */
	bool Write(const void *data, size_t size, Error &error) override;
};

#endif
