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

#ifndef QOBUZ_LOGIN_REQUEST_HXX
#define QOBUZ_LOGIN_REQUEST_HXX

#include "check.h"
#include "QobuzSession.hxx"
#include "lib/curl/Handler.hxx"
#include "lib/curl/Request.hxx"
#include "lib/yajl/Handle.hxx"

#include <exception>
#include <memory>
#include <string>

class CurlRequest;
class QobuzErrorParser;

class QobuzLoginHandler {
public:
	virtual void OnQobuzLoginSuccess(QobuzSession &&session) noexcept = 0;
	virtual void OnQobuzLoginError(std::exception_ptr error) noexcept = 0;
};

class QobuzLoginRequest final : CurlResponseHandler {
	CurlRequest request;

	std::unique_ptr<QobuzErrorParser> error_parser;

	Yajl::Handle parser;

	enum class State {
		NONE,
		DEVICE,
		DEVICE_ID,
		USER_AUTH_TOKEN,
	} state = State::NONE;

	unsigned map_depth = 0;

	QobuzSession session;

	std::exception_ptr error;

	QobuzLoginHandler &handler;

public:
	QobuzLoginRequest(CurlGlobal &curl,
			  const char *base_url, const char *app_id,
			  const char *username, const char *email,
			  const char *password,
			  const char *device_manufacturer_id,
			  QobuzLoginHandler &_handler) noexcept;

	~QobuzLoginRequest() noexcept;

	void Start() noexcept {
		request.StartIndirect();
	}

private:
	/* virtual methods from CurlResponseHandler */
	void OnHeaders(unsigned status,
		       std::multimap<std::string, std::string> &&headers) override;
	void OnData(ConstBuffer<void> data) override;
	void OnEnd() override;
	void OnError(std::exception_ptr e) noexcept override;

public:
	/* yajl callbacks */
	bool String(StringView value) noexcept;
	bool StartMap() noexcept;
	bool MapKey(StringView value) noexcept;
	bool EndMap() noexcept;
};

#endif
