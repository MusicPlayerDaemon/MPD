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

#include "QobuzLoginRequest.hxx"
#include "QobuzErrorParser.hxx"
#include "QobuzSession.hxx"
#include "lib/curl/Form.hxx"
#include "lib/yajl/Callbacks.hxx"

#include <cassert>

using Wrapper = Yajl::CallbacksWrapper<QobuzLoginRequest::ResponseParser>;
static constexpr yajl_callbacks parse_callbacks = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	Wrapper::String,
	Wrapper::StartMap,
	Wrapper::MapKey,
	Wrapper::EndMap,
	nullptr,
	nullptr,
};

class QobuzLoginRequest::ResponseParser final : public YajlResponseParser {
	enum class State {
		NONE,
		DEVICE,
		DEVICE_ID,
		USER_AUTH_TOKEN,
	} state = State::NONE;

	unsigned map_depth = 0;

	QobuzSession session;

public:
	explicit ResponseParser() noexcept
		:YajlResponseParser(&parse_callbacks, nullptr, this) {}

	QobuzSession &&GetSession();

	/* yajl callbacks */
	bool String(StringView value) noexcept;
	bool StartMap() noexcept;
	bool MapKey(StringView value) noexcept;
	bool EndMap() noexcept;
};

inline QobuzSession &&
QobuzLoginRequest::ResponseParser::GetSession()
{
	if (session.user_auth_token.empty())
		throw std::runtime_error("No user_auth_token in login response");

	if (session.device_id.empty())
		throw std::runtime_error("No device id in login response");

	return std::move(session);
}

static Curl::Headers
MakeLoginForm(const char *app_id,
	      const char *username, const char *email,
	      const char *password,
	      const char *device_manufacturer_id)
{
	assert(username != nullptr || email != nullptr);

	Curl::Headers form{
		{"app_id", app_id},
		{"password", password},
		{"device_manufacturer_id", device_manufacturer_id},
	};

	if (username != nullptr)
		form.emplace("username", username);
	else
		form.emplace("email", email);

	return form;
}

static std::string
MakeLoginUrl(CURL *curl,
	     const char *base_url, const char *app_id,
	     const char *username, const char *email,
	     const char *password,
	     const char *device_manufacturer_id)
{
	std::string url(base_url);
	url += "user/login?";
	url += EncodeForm(curl,
			  MakeLoginForm(app_id, username, email, password,
					device_manufacturer_id));
	return url;
}

QobuzLoginRequest::QobuzLoginRequest(CurlGlobal &curl,
				     const char *base_url, const char *app_id,
				     const char *username, const char *email,
				     const char *password,
				     const char *device_manufacturer_id,
				     QobuzLoginHandler &_handler)
	:request(curl, *this),
	 handler(_handler)
{
	request.SetUrl(MakeLoginUrl(request.Get(), base_url, app_id,
				    username, email, password,
				    device_manufacturer_id).c_str());
}

QobuzLoginRequest::~QobuzLoginRequest() noexcept
{
	request.StopIndirect();
}

std::unique_ptr<CurlResponseParser>
QobuzLoginRequest::MakeParser(unsigned status, Curl::Headers &&headers)
{
	if (status != 200)
		return std::make_unique<QobuzErrorParser>(status, headers);

	auto i = headers.find("content-type");
	if (i == headers.end() || i->second.find("/json") == i->second.npos)
		throw std::runtime_error("Not a JSON response from Qobuz");

	return std::make_unique<ResponseParser>();
}

void
QobuzLoginRequest::FinishParser(std::unique_ptr<CurlResponseParser> p)
{
	assert(dynamic_cast<ResponseParser *>(p.get()) != nullptr);
	auto &rp = (ResponseParser &)*p;
	handler.OnQobuzLoginSuccess(rp.GetSession());
}

void
QobuzLoginRequest::OnError(std::exception_ptr e) noexcept
{
	handler.OnQobuzLoginError(e);
}

inline bool
QobuzLoginRequest::ResponseParser::String(StringView value) noexcept
{
	switch (state) {
	case State::NONE:
	case State::DEVICE:
		break;

	case State::DEVICE_ID:
		session.device_id.assign(value.data, value.size);
		break;

	case State::USER_AUTH_TOKEN:
		session.user_auth_token.assign(value.data, value.size);
		break;
	}

	return true;
}

inline bool
QobuzLoginRequest::ResponseParser::StartMap() noexcept
{
	switch (state) {
	case State::NONE:
		break;

	case State::DEVICE:
	case State::DEVICE_ID:
		++map_depth;
		break;

	case State::USER_AUTH_TOKEN:
		break;
	}

	return true;
}

inline bool
QobuzLoginRequest::ResponseParser::MapKey(StringView value) noexcept
{
	switch (state) {
	case State::NONE:
		if (value.Equals("user_auth_token"))
			state = State::USER_AUTH_TOKEN;
		else if (value.Equals("device")) {
			state = State::DEVICE;
			map_depth = 0;
		}

		break;

	case State::DEVICE:
		if (value.Equals("id"))
			state = State::DEVICE_ID;
		break;

	case State::DEVICE_ID:
		break;

	case State::USER_AUTH_TOKEN:
		break;
	}


	return true;
}

inline bool
QobuzLoginRequest::ResponseParser::EndMap() noexcept
{
	switch (state) {
	case State::NONE:
		break;

	case State::DEVICE_ID:
		state = State::DEVICE;
		break;

	case State::DEVICE:
	case State::USER_AUTH_TOKEN:
		break;
	}

	switch (state) {
	case State::NONE:
	case State::DEVICE_ID:
		break;

	case State::DEVICE:
		assert(map_depth > 0);
		if (--map_depth == 0)
			state = State::NONE;
		break;

	case State::USER_AUTH_TOKEN:
		break;
	}

	return true;
}
