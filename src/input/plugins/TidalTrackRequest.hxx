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

#ifndef TIDAL_TRACK_REQUEST_HXX
#define TIDAL_TRACK_REQUEST_HXX

#include "check.h"
#include "lib/curl/Handler.hxx"
#include "lib/curl/Slist.hxx"
#include "lib/curl/Request.hxx"
#include "lib/yajl/Handle.hxx"

#include <exception>
#include <string>

class CurlRequest;

class TidalTrackHandler
	: public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::safe_link>>
{
public:
	virtual void OnTidalTrackSuccess(std::string &&url) noexcept = 0;
	virtual void OnTidalTrackError(std::exception_ptr error) noexcept = 0;
};

class TidalTrackRequest final : CurlResponseHandler {
	CurlSlist request_headers;

	CurlRequest request;

	Yajl::Handle parser;

	enum class State {
		NONE,
		URLS,
	} state = State::NONE;

	std::string url;

	std::exception_ptr error;

	TidalTrackHandler &handler;

public:
	TidalTrackRequest(CurlGlobal &curl,
			  const char *base_url, const char *token,
			  const char *session,
			  const char *track_id,
			  TidalTrackHandler &_handler) noexcept;

	~TidalTrackRequest() noexcept;

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
	bool MapKey(StringView value) noexcept;
	bool EndMap() noexcept;
};

#endif
