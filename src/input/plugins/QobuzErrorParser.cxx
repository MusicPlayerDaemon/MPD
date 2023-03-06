// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "QobuzErrorParser.hxx"
#include "lib/yajl/Callbacks.hxx"
#include "lib/fmt/RuntimeError.hxx"

using std::string_view_literals::operator""sv;

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
		throw FmtRuntimeError("Status {} from Qobuz", status);
}

void
QobuzErrorParser::OnEnd()
{
	YajlResponseParser::OnEnd();

	if (!message.empty())
		throw FmtRuntimeError("Error from Qobuz: {}", message);
	else
		throw FmtRuntimeError("Status {} from Qobuz", status);
}

inline bool
QobuzErrorParser::String(std::string_view value) noexcept
{
	switch (state) {
	case State::NONE:
		break;

	case State::MESSAGE:
		message = value;
		break;
	}

	return true;
}

inline bool
QobuzErrorParser::MapKey(std::string_view value) noexcept
{
	if (value == "message"sv)
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
