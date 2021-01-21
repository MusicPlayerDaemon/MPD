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

#ifndef MPD_REQUEST_HXX
#define MPD_REQUEST_HXX

#include "protocol/ArgParser.hxx"
#include "protocol/RangeArg.hxx"
#include "Chrono.hxx"
#include "util/ConstBuffer.hxx"

#include <cassert>
#include <utility>

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

	int ParseInt(unsigned idx) const {
		assert(idx < size);
		return ParseCommandArgInt(data[idx]);
	}

	int ParseInt(unsigned idx, int min_value, int max_value) const {
		assert(idx < size);
		return ParseCommandArgInt(data[idx], min_value, max_value);
	}

	unsigned ParseUnsigned(unsigned idx) const {
		assert(idx < size);
		return ParseCommandArgUnsigned(data[idx]);
	}

	unsigned ParseUnsigned(unsigned idx, unsigned max_value) const {
		assert(idx < size);
		return ParseCommandArgUnsigned(data[idx], max_value);
	}

	bool ParseBool(unsigned idx) const {
		assert(idx < size);
		return ParseCommandArgBool(data[idx]);
	}

	RangeArg ParseRange(unsigned idx) const {
		assert(idx < size);
		return ParseCommandArgRange(data[idx]);
	}

	float ParseFloat(unsigned idx) const {
		assert(idx < size);
		return ParseCommandArgFloat(data[idx]);
	}

	SongTime ParseSongTime(unsigned idx) const {
		assert(idx < size);
		return ParseCommandArgSongTime(data[idx]);
	}

	SignedSongTime ParseSignedSongTime(unsigned idx) const {
		assert(idx < size);
		return ParseCommandArgSignedSongTime(data[idx]);
	}

	int ParseOptional(unsigned idx, int default_value) const {
		return idx < size
			? ParseInt(idx)
			: default_value;
	}

	RangeArg ParseOptional(unsigned idx, RangeArg default_value) const {
		return idx < size
			? ParseRange(idx)
			: default_value;
	}
};

#endif
