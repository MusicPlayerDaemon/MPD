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

#ifndef QOBUZ_LOGIN_REQUEST_HXX
#define QOBUZ_LOGIN_REQUEST_HXX

#include "lib/curl/Delegate.hxx"
#include "lib/curl/Request.hxx"

class CurlRequest;
struct QobuzSession;

class QobuzLoginHandler {
public:
	virtual void OnQobuzLoginSuccess(QobuzSession &&session) noexcept = 0;
	virtual void OnQobuzLoginError(std::exception_ptr error) noexcept = 0;
};

class QobuzLoginRequest final : DelegateCurlResponseHandler {
	CurlRequest request;

	QobuzLoginHandler &handler;

public:
	class ResponseParser;

	QobuzLoginRequest(CurlGlobal &curl,
			  const char *base_url, const char *app_id,
			  const char *username, const char *email,
			  const char *password,
			  const char *device_manufacturer_id,
			  QobuzLoginHandler &_handler);

	~QobuzLoginRequest() noexcept;

	void Start() noexcept {
		request.StartIndirect();
	}

private:
	/* virtual methods from DelegateCurlResponseHandler */
	std::unique_ptr<CurlResponseParser> MakeParser(unsigned status,
						       Curl::Headers &&headers) override;
	void FinishParser(std::unique_ptr<CurlResponseParser> p) override;

	/* virtual methods from CurlResponseHandler */
	void OnError(std::exception_ptr e) noexcept override;
};

#endif
