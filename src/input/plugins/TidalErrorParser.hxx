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

#ifndef TIDAL_ERROR_PARSER_HXX
#define TIDAL_ERROR_PARSER_HXX

#include "lib/yajl/ResponseParser.hxx"

#include <string>
#include <map>

template<typename T> struct ConstBuffer;
struct StringView;

/**
 * Parse an error JSON response and throw a #TidalError upon
 * completion.
 */
class TidalErrorParser final : public YajlResponseParser {
	const unsigned status;

	enum class State {
		NONE,
		USER_MESSAGE,
		SUB_STATUS,
	} state = State::NONE;

	unsigned sub_status = 0;

	std::string message;

public:
	/**
	 * May throw if there is a formal error in the response
	 * headers.
	 */
	TidalErrorParser(unsigned status,
			 const std::multimap<std::string, std::string> &headers);

protected:
	/* virtual methods from CurlResponseParser */
	void OnEnd() override;

public:
	/* yajl callbacks */
	bool Integer(long long value) noexcept;
	bool String(StringView value) noexcept;
	bool MapKey(StringView value) noexcept;
	bool EndMap() noexcept;
};

#endif
