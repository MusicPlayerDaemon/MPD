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

#ifndef MPD_GUNZIP_READER_HXX
#define MPD_GUNZIP_READER_HXX

#include "check.h"
#include "Reader.hxx"
#include "util/StaticFifoBuffer.hxx"
#include "Compiler.h"

#include <zlib.h>

class Error;
class Domain;

/**
 * A filter that decompresses data using zlib.
 */
class GunzipReader final : public Reader {
	Reader &next;

	bool eof;

	z_stream z;

	StaticFifoBuffer<Bytef, 4096> buffer;

public:
	/**
	 * Construct the filter.  Call IsDefined() to check whether
	 * the constructor has succeeded.  If not, #error will hold
	 * information about the failure.
	 */
	GunzipReader(Reader &_next, Error &error);
	~GunzipReader();

	/**
	 * Check whether the constructor has succeeded.
	 */
	bool IsDefined() const {
		return z.opaque == nullptr;
	}

	/* virtual methods from class Reader */
	virtual size_t Read(void *data, size_t size, Error &error) override;

private:
	bool FillBuffer(Error &error);
};

#endif
