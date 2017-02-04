/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

#ifndef MPD_REQUEST_HXX
#define MPD_REQUEST_HXX

#include "check.h"
#include "protocol/ArgParser.hxx"
#include "Chrono.hxx"
#include "util/ConstBuffer.hxx"

#include <utility>

#include <assert.h>

class Response;

class Request : public ConstBuffer<const char *> {
	typedef ConstBuffer<const char *> Base;

public:
	constexpr Request(const char *const*argv, size_type n)
		:Base(argv, n) {}

	constexpr const char *GetOptional(unsigned idx,
					  const char *default_value=nullptr) const {
		return idx < size
			     ? data[idx]
			     : default_value;
	}

	gcc_pure
	int ParseInt(unsigned idx) const {
		assert(idx < size);
		return ParseCommandArgInt(data[idx]);
	}

	gcc_pure
	int ParseInt(unsigned idx, int min_value, int max_value) const {
		assert(idx < size);
		return ParseCommandArgInt(data[idx], min_value, max_value);
	}

	gcc_pure
	int ParseUnsigned(unsigned idx) const {
		assert(idx < size);
		return ParseCommandArgUnsigned(data[idx]);
	}

	gcc_pure
	int ParseUnsigned(unsigned idx, unsigned max_value) const {
		assert(idx < size);
		return ParseCommandArgUnsigned(data[idx], max_value);
	}

	gcc_pure
	bool ParseBool(unsigned idx) const {
		assert(idx < size);
		return ParseCommandArgBool(data[idx]);
	}

	gcc_pure
	RangeArg ParseRange(unsigned idx) const {
		assert(idx < size);
		return ParseCommandArgRange(data[idx]);
	}

	gcc_pure
	float ParseFloat(unsigned idx) const {
		assert(idx < size);
		return ParseCommandArgFloat(data[idx]);
	}

	gcc_pure
	SongTime ParseSongTime(unsigned idx) const {
		assert(idx < size);
		return ParseCommandArgSongTime(data[idx]);
	}

	gcc_pure
	SignedSongTime ParseSignedSongTime(unsigned idx) const {
		assert(idx < size);
		return ParseCommandArgSignedSongTime(data[idx]);
	}

	gcc_pure
	int ParseOptional(unsigned idx, int default_value) const {
		return idx < size
			? ParseInt(idx)
			: default_value;
	}

	gcc_pure
	RangeArg ParseOptional(unsigned idx, RangeArg default_value) const {
		return idx < size
			? ParseRange(idx)
			: default_value;
	}
};

#endif
