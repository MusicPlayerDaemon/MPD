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

#ifndef TIDAL_TAG_SCANNER_HXX
#define TIDAL_TAG_SCANNER_HXX

#include "lib/curl/Delegate.hxx"
#include "lib/curl/Slist.hxx"
#include "lib/curl/Request.hxx"
#include "input/RemoteTagScanner.hxx"

class TidalTagScanner final
	: public RemoteTagScanner, DelegateCurlResponseHandler
{
	CurlSlist request_headers;

	CurlRequest request;

	RemoteTagHandler &handler;

public:
	class ResponseParser;

	TidalTagScanner(CurlGlobal &curl,
			const char *base_url, const char *token,
			const char *track_id,
			RemoteTagHandler &_handler);

	~TidalTagScanner() noexcept override;

	void Start() override {
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
