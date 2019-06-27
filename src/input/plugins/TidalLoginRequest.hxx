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

#ifndef TIDAL_LOGIN_REQUEST_HXX
#define TIDAL_LOGIN_REQUEST_HXX

#include "lib/curl/Delegate.hxx"
#include "lib/curl/Slist.hxx"
#include "lib/curl/Request.hxx"

/**
 * Callback class for #TidalLoginRequest.
 *
 * Its methods must be thread-safe.
 */
class TidalLoginHandler {
public:
	virtual void OnTidalLoginSuccess(std::string session) noexcept = 0;
	virtual void OnTidalLoginError(std::exception_ptr error) noexcept = 0;
};

/**
 * An asynchronous Tidal "login/username" request.
 *
 * After construction, call Start() to initiate the request.
 */
class TidalLoginRequest final : DelegateCurlResponseHandler {
	CurlSlist request_headers;

	CurlRequest request;

	TidalLoginHandler &handler;

public:
	class ResponseParser;

	TidalLoginRequest(CurlGlobal &curl,
			  const char *base_url, const char *token,
			  const char *username, const char *password,
			  TidalLoginHandler &_handler);

	~TidalLoginRequest() noexcept;

	void Start() {
		request.StartIndirect();
	}

private:
	/* virtual methods from DelegateCurlResponseHandler */
	std::unique_ptr<CurlResponseParser> MakeParser(unsigned status,
						       std::multimap<std::string, std::string> &&headers) override;
	void FinishParser(std::unique_ptr<CurlResponseParser> p) override;

	/* virtual methods from CurlResponseHandler */
	void OnError(std::exception_ptr e) noexcept override;
};

#endif
