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

#include "TidalErrorParser.hxx"
#include "TidalError.hxx"
#include "lib/yajl/Callbacks.hxx"
#include "util/RuntimeError.hxx"

using Wrapper = Yajl::CallbacksWrapper<TidalErrorParser>;
static constexpr yajl_callbacks tidal_error_parser_callbacks = {
	nullptr,
	nullptr,
	Wrapper::Integer,
	nullptr,
	nullptr,
	Wrapper::String,
	nullptr,
	Wrapper::MapKey,
	Wrapper::EndMap,
	nullptr,
	nullptr,
};

TidalErrorParser::TidalErrorParser(unsigned _status,
				   const std::multimap<std::string, std::string> &headers)
	:YajlResponseParser(&tidal_error_parser_callbacks, nullptr, this),
	 status(_status)
{
	auto i = headers.find("content-type");
	if (i == headers.end() || i->second.find("/json") == i->second.npos)
		throw FormatRuntimeError("Status %u from Tidal", status);
}

void
TidalErrorParser::OnEnd()
{
	YajlResponseParser::OnEnd();

	char what[1024];

	if (!message.empty())
		snprintf(what, sizeof(what), "Error from Tidal: %s",
			 message.c_str());
	else
		snprintf(what, sizeof(what), "Status %u from Tidal", status);

	throw TidalError(status, sub_status, what);
}

inline bool
TidalErrorParser::Integer(long long value) noexcept
{
	switch (state) {
	case State::NONE:
	case State::USER_MESSAGE:
		break;

	case State::SUB_STATUS:
		sub_status = value;
		break;
	}

	return true;
}

inline bool
TidalErrorParser::String(StringView value) noexcept
{
	switch (state) {
	case State::NONE:
	case State::SUB_STATUS:
		break;

	case State::USER_MESSAGE:
		message.assign(value.data, value.size);
		break;
	}

	return true;
}

inline bool
TidalErrorParser::MapKey(StringView value) noexcept
{
	if (value.Equals("userMessage"))
		state = State::USER_MESSAGE;
	else if (value.Equals("subStatus"))
		state = State::SUB_STATUS;
	else
		state = State::NONE;

	return true;
}

inline bool
TidalErrorParser::EndMap() noexcept
{
	state = State::NONE;

	return true;
}
