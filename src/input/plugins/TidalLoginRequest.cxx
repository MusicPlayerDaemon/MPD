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

#include "config.h"
#include "TidalLoginRequest.hxx"
#include "TidalErrorParser.hxx"
#include "lib/curl/Form.hxx"
#include "lib/yajl/Callbacks.hxx"
#include "util/RuntimeError.hxx"

using Wrapper = Yajl::CallbacksWrapper<TidalLoginRequest>;
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

static std::string
MakeLoginUrl(const char *base_url)
{
	return std::string(base_url) + "/login/username";
}

TidalLoginRequest::TidalLoginRequest(CurlGlobal &curl,
				     const char *base_url, const char *token,
				     const char *username, const char *password,
				     TidalLoginHandler &_handler) noexcept
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

void
TidalLoginRequest::OnHeaders(unsigned status,
			     std::multimap<std::string, std::string> &&headers)
{
	if (status != 200) {
		error_parser = std::make_unique<TidalErrorParser>(status, headers);
		return;
	}

	auto i = headers.find("content-type");
	if (i == headers.end() || i->second.find("/json") == i->second.npos)
		throw std::runtime_error("Not a JSON response from Tidal");

	parser = {&parse_callbacks, nullptr, this};
}

void
TidalLoginRequest::OnData(ConstBuffer<void> data)
{
	if (error_parser) {
		error_parser->OnData(data);
		return;
	}

	parser.Parse((const unsigned char *)data.data, data.size);
}

void
TidalLoginRequest::OnEnd()
{
	if (error_parser) {
		error_parser->OnEnd();
		return;
	}

	parser.CompleteParse();

	if (session.empty())
		throw std::runtime_error("No sessionId in login response");

	handler.OnTidalLoginSuccess(std::move(session));
}

void
TidalLoginRequest::OnError(std::exception_ptr e) noexcept
{
	handler.OnTidalLoginError(e);
}

inline bool
TidalLoginRequest::String(StringView value) noexcept
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
TidalLoginRequest::MapKey(StringView value) noexcept
{
	if (value.Equals("sessionId"))
		state = State::SESSION_ID;
	else
		state = State::NONE;

	return true;
}

inline bool
TidalLoginRequest::EndMap() noexcept
{
	state = State::NONE;

	return true;
}
