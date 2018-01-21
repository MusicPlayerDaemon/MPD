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
#include "QobuzLoginRequest.hxx"
#include "QobuzErrorParser.hxx"
#include "lib/curl/Form.hxx"
#include "lib/yajl/Callbacks.hxx"
#include "util/RuntimeError.hxx"

using Wrapper = Yajl::CallbacksWrapper<QobuzLoginRequest>;
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

static std::multimap<std::string, std::string>
MakeLoginForm(const char *app_id,
	      const char *username, const char *email,
	      const char *password,
	      const char *device_manufacturer_id)
{
	assert(username != nullptr || email != nullptr);

	std::multimap<std::string, std::string> form{
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
				     QobuzLoginHandler &_handler) noexcept
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

void
QobuzLoginRequest::OnHeaders(unsigned status,
			     std::multimap<std::string, std::string> &&headers)
{
	if (status != 200) {
		error_parser = std::make_unique<QobuzErrorParser>(status, headers);
		return;
	}

	auto i = headers.find("content-type");
	if (i == headers.end() || i->second.find("/json") == i->second.npos)
		throw std::runtime_error("Not a JSON response from Qobuz");

	parser = {&parse_callbacks, nullptr, this};
}

void
QobuzLoginRequest::OnData(ConstBuffer<void> data)
{
	if (error_parser) {
		error_parser->OnData(data);
		return;
	}

	parser.Parse((const unsigned char *)data.data, data.size);
}

void
QobuzLoginRequest::OnEnd()
{
	if (error_parser) {
		error_parser->OnEnd();
		return;
	}

	parser.CompleteParse();

	if (session.user_auth_token.empty())
		throw std::runtime_error("No user_auth_token in login response");

	if (session.device_id.empty())
		throw std::runtime_error("No device id in login response");

	handler.OnQobuzLoginSuccess(std::move(session));
}

void
QobuzLoginRequest::OnError(std::exception_ptr e) noexcept
{
	handler.OnQobuzLoginError(e);
}

inline bool
QobuzLoginRequest::String(StringView value) noexcept
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
QobuzLoginRequest::StartMap() noexcept
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
QobuzLoginRequest::MapKey(StringView value) noexcept
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
QobuzLoginRequest::EndMap() noexcept
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
