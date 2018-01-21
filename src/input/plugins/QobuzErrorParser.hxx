/*
 * Copyright 2003-2018 The Music Player Daemon Project
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

#ifndef QOBUZ_ERROR_PARSER_HXX
#define QOBUZ_ERROR_PARSER_HXX

#include "check.h"
#include "lib/yajl/Handle.hxx"

#include <exception>
#include <string>
#include <map>

template<typename T> struct ConstBuffer;
struct StringView;

/**
 * Parse an error JSON response.
 */
class QobuzErrorParser {
	const unsigned status;

	Yajl::Handle parser;

	enum class State {
		NONE,
		MESSAGE,
	} state = State::NONE;

	std::string message;

public:
	/**
	 * May throw if there is a formal error in the response
	 * headers.
	 */
	QobuzErrorParser(unsigned status,
			 const std::multimap<std::string, std::string> &headers);

	/**
	 * Feed response body data into the JSON parser.
	 */
	void OnData(ConstBuffer<void> data);

	/**
	 * Throw an exception describing the error condition.  Call
	 * this at the end of the response body.
	 */
	void OnEnd();

public:
	/* yajl callbacks */
	bool String(StringView value) noexcept;
	bool MapKey(StringView value) noexcept;
	bool EndMap() noexcept;
};

#endif
