// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "QobuzLoginRequest.hxx"
#include "QobuzErrorParser.hxx"
#include "QobuzSession.hxx"
#include "lib/curl/Form.hxx"

#include <fmt/core.h>
#include <nlohmann/json.hpp>

#include <cassert>

using std::string_view_literals::operator""sv;

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
	return fmt::format("{}user/login?{}"sv, base_url,
			   EncodeForm(curl,
				      MakeLoginForm(app_id, username, email, password,
						    device_manufacturer_id)));
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
	request.GetEasy().SetURL(MakeLoginUrl(request.Get(), base_url, app_id,
					      username, email, password,
					      device_manufacturer_id).c_str());
}

QobuzLoginRequest::~QobuzLoginRequest() noexcept
{
	request.StopIndirect();
}

static void
from_json(const nlohmann::json &j, QobuzSession &session)
{
	j.at("user_auth_token"sv).get_to(session.user_auth_token);
	if (session.user_auth_token.empty())
		throw std::runtime_error("No user_auth_token in login response");
}

void
QobuzLoginRequest::OnEnd()
{
	const auto &r = GetResponse();
	if (r.status != 200)
		ThrowQobuzError(r);

	if (auto i = r.headers.find("content-type");
	    i == r.headers.end() || i->second.find("/json") == i->second.npos)
		throw std::runtime_error("Not a JSON response from Qobuz");

	handler.OnQobuzLoginSuccess(nlohmann::json::parse(r.body));
}

void
QobuzLoginRequest::OnError(std::exception_ptr e) noexcept
{
	handler.OnQobuzLoginError(e);
}
