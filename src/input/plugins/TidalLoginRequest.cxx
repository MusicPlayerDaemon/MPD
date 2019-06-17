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

#include "TidalLoginRequest.hxx"
#include "TidalErrorParser.hxx"
#include "lib/curl/Form.hxx"
#include "lib/yajl/Callbacks.hxx"
#include "lib/yajl/ResponseParser.hxx"

using Wrapper = Yajl::CallbacksWrapper<TidalLoginRequest::ResponseParser>;
static constexpr yajl_callbacks parse_callbacks = {
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

class TidalLoginRequest::ResponseParser final : public YajlResponseParser {
	enum class State {
		NONE,
		SESSION_ID,
	} state = State::NONE;

	std::string session;

public:
	explicit ResponseParser() noexcept
		:YajlResponseParser(&parse_callbacks, nullptr, this) {}

	std::string &&GetSession() {
		if (session.empty())
			throw std::runtime_error("No sessionId in login response");

		return std::move(session);
	}

	/* yajl callbacks */
	bool String(StringView value) noexcept;
	bool MapKey(StringView value) noexcept;
	bool EndMap() noexcept;
};

static std::string
MakeLoginUrl(const char *base_url)
{
	return std::string(base_url) + "/login/username";
}

TidalLoginRequest::TidalLoginRequest(CurlGlobal &curl,
				     const char *base_url, const char *token,
				     const char *username, const char *password,
				     TidalLoginHandler &_handler)
	:request(curl, MakeLoginUrl(base_url).c_str(), *this),
	 handler(_handler)
{
	request_headers.Append((std::string("X-Tidal-Token:")
				+ token).c_str());
	request.SetOption(CURLOPT_HTTPHEADER, request_headers.Get());

	request.SetOption(CURLOPT_COPYPOSTFIELDS,
			  EncodeForm(request.Get(),
				     {{"username", username}, {"password", password}}).c_str());
}

TidalLoginRequest::~TidalLoginRequest() noexcept
{
	request.StopIndirect();
}

std::unique_ptr<CurlResponseParser>
TidalLoginRequest::MakeParser(unsigned status,
			      std::multimap<std::string, std::string> &&headers)
{
	if (status != 200)
		return std::make_unique<TidalErrorParser>(status, headers);

	auto i = headers.find("content-type");
	if (i == headers.end() || i->second.find("/json") == i->second.npos)
		throw std::runtime_error("Not a JSON response from Tidal");

	return std::make_unique<ResponseParser>();
}

void
TidalLoginRequest::FinishParser(std::unique_ptr<CurlResponseParser> p)
{
	assert(dynamic_cast<ResponseParser *>(p.get()) != nullptr);
	auto &rp = (ResponseParser &)*p;
	handler.OnTidalLoginSuccess(rp.GetSession());
}

void
TidalLoginRequest::OnError(std::exception_ptr e) noexcept
{
	handler.OnTidalLoginError(e);
}

inline bool
TidalLoginRequest::ResponseParser::String(StringView value) noexcept
{
	switch (state) {
	case State::NONE:
		break;

	case State::SESSION_ID:
		session.assign(value.data, value.size);
		break;
	}

	return true;
}

inline bool
TidalLoginRequest::ResponseParser::MapKey(StringView value) noexcept
{
	if (value.Equals("sessionId"))
		state = State::SESSION_ID;
	else
		state = State::NONE;

	return true;
}

inline bool
TidalLoginRequest::ResponseParser::EndMap() noexcept
{
	state = State::NONE;

	return true;
}
