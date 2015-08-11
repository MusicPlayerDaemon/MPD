/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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
#include "util/ConstBuffer.hxx"

#include <utility>

#include <assert.h>

class Client;

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

	template<typename T, typename... Args>
	bool Parse(unsigned idx, T &value_r, Client &client,
		   Args&&... args) {
		assert(idx < size);

		return ParseCommandArg(client, value_r, data[idx],
				       std::forward<Args>(args)...);
	}

	template<typename T, typename... Args>
	bool ParseOptional(unsigned idx, T &value_r, Client &client,
			   Args&&... args) {
		return idx >= size ||
			Parse(idx, value_r, client,
			      std::forward<Args>(args)...);
	}

	template<typename T, typename... Args>
	bool ParseShift(unsigned idx, T &value_r, Client &client,
			Args&&... args) {
		bool success = Parse(idx, value_r, client,
				     std::forward<Args>(args)...);
		shift();
		return success;
	}
};

#endif
