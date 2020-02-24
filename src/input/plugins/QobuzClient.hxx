/*
 * Copyright 2018 Goldhorn
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

#pragma once

#include "check.h"
#include "QobuzSession.hxx"
#include "lib/curl/Init.hxx"

#include <map>

class QobuzClient {
	QobuzSession session;
	CurlInit curl;
	const char *base_url;

public:
	QobuzClient(EventLoop &event_loop,
		    const char *_base_url);

	gcc_pure
	CurlGlobal &GetCurl() noexcept;

	QobuzSession &GetSession() noexcept;

	std::string GetFormatId() const noexcept;

	std::string MakeUrl(const char *object, const char *method,
			    const std::multimap<std::string, std::string> &query) const noexcept;

	std::string MakeSignedUrl(const char *object, const char *method,
				  const std::multimap<std::string, std::string> &query) const noexcept;

};
