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

#include "QobuzErrorParser.hxx"
#include "lib/yajl/Callbacks.hxx"
#include "util/ConstBuffer.hxx"
#include "util/RuntimeError.hxx"

using Wrapper = Yajl::CallbacksWrapper<QobuzErrorParser>;
static constexpr yajl_callbacks qobuz_error_parser_callbacks = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	Wrapper::String,
	nullptr,
	Wrapper::MapKey,
	Wrapper::EndMap,
	nullptr,
	nullptr,
};

QobuzErrorParser::QobuzErrorParser(unsigned _status,
				   const Curl::Headers &headers)
	:YajlResponseParser(&qobuz_error_parser_callbacks, nullptr, this),
	 status(_status)
{
	auto i = headers.find("content-type");
	if (i == headers.end() || i->second.find("/json") == i->second.npos)
		throw FormatRuntimeError("Status %u from Qobuz", status);
}

void
QobuzErrorParser::OnEnd()
{
	YajlResponseParser::OnEnd();

	if (!message.empty())
		throw FormatRuntimeError("Error from Qobuz: %s",
					 message.c_str());
	else
		throw FormatRuntimeError("Status %u from Qobuz", status);
}

inline bool
QobuzErrorParser::String(StringView value) noexcept
{
	switch (state) {
	case State::NONE:
		break;

	case State::MESSAGE:
		message.assign(value.data, value.size);
		break;
	}

	return true;
}

inline bool
QobuzErrorParser::MapKey(StringView value) noexcept
{
	if (value.Equals("message"))
		state = State::MESSAGE;
	else
		state = State::NONE;

	return true;
}

inline bool
QobuzErrorParser::EndMap() noexcept
{
	state = State::NONE;

	return true;
}
