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
#include "QobuzClient.hxx"
#include "lib/gcrypt/MD5.hxx"
#include "util/ConstBuffer.hxx"

#include <stdexcept>

#include <assert.h>

namespace {

class QueryStringBuilder {
	bool first = true;

public:
	QueryStringBuilder &operator()(std::string &dest, const char *name,
				       const char *value) noexcept {
		dest.push_back(first ? '?' : '&');
		first = false;

		dest += name;
		dest.push_back('=');
		dest += value; // TODO: escape

		return *this;
	}
};

}

QobuzClient::QobuzClient(EventLoop &event_loop, const char *_base_url)
	: curl(event_loop)
	, base_url(_base_url)
{
}

CurlGlobal &
QobuzClient::GetCurl() noexcept
{
	return *curl;
}

QobuzSession &
QobuzClient::GetSession() noexcept
{
	return session;
}

std::string
QobuzClient::GetFormatId() const noexcept
{
	return std::to_string(session.format_id);
}

std::string
QobuzClient::MakeUrl(const char *object, const char *method,
		     const std::multimap<std::string, std::string> &query) const noexcept
{
	assert(!query.empty());

	std::string uri(base_url);
	uri += object;
	uri.push_back('/');
	uri += method;

	QueryStringBuilder q;
	for (const auto &i : query)
		q(uri, i.first.c_str(), i.second.c_str());

	q(uri, "app_id", session.app_id.c_str());
	return uri;
}

std::string
QobuzClient::MakeSignedUrl(const char *object, const char *method,
			   const std::multimap<std::string, std::string> &query) const noexcept
{
	assert(!query.empty());

	std::string uri(base_url);
	uri += object;
	uri.push_back('/');
	uri += method;

	QueryStringBuilder q;
	std::string concatenated_query(object);
	concatenated_query += method;
	for (const auto &i : query) {
		q(uri, i.first.c_str(), i.second.c_str());

		concatenated_query += i.first;
		concatenated_query += i.second;
	}

	q(uri, "app_id", session.app_id.c_str());

	const auto request_ts = std::to_string(time(nullptr));
	q(uri, "request_ts", request_ts.c_str());
	concatenated_query += request_ts;

	concatenated_query += session.app_secret;

	const auto md5_hex = MD5Hex({concatenated_query.data(), concatenated_query.size()});
	q(uri, "request_sig", &md5_hex.front());

	return uri;
}

