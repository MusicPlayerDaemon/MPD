/*
 * Copyright 2015-2018 Cary Audio
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

#include "CurlCommand.hxx"
#include "external/jaijson/jaijson.hxx"
#include "HttpError.hxx"

class CurlSocket
{
public:
	CurlSocket();
	template<typename T>
	static inline void post(const std::string &url, const std::string &data, T &out) {
		request(CurlCommand::POST, url, data, std::string(), out);
	}

	template<typename T>
	static inline void post(const std::string &url, T &out) {
		request(CurlCommand::POST, url, std::string(), std::string(), out);
	}

	template<typename T>
	static inline void get(const std::string &url, T &out) {
		request(CurlCommand::GET, url, std::string(), std::string(), out);
	}

	template<typename T>
	static inline void del(const std::string &url, T &out) {
		request(CurlCommand::DELETE, url.c_str(), std::string(), std::string(), out);
	}

	static CurlRespond request(const CurlCommand &cmd);

	static inline CurlRespond request(CurlCommand::Command _cmd,
		const std::string &url,
		const std::string &data,
		const std::string &etag) {
		CurlCommand cmd(_cmd, url, data, etag);
		return request(cmd);
	}

	template<typename T>
	static void	request(const CurlCommand &cmd, T &out)
	{
		CurlRespond respond = request(cmd);
		if (!respond.etag.empty()) {
			out.etag = respond.etag;
		}
		jaijson::Document doc;
		if (!respond.rxdata.empty()) {
			if (doc.Parse(respond.rxdata.c_str()).HasParseError()) {
				throw HttpError::unexpectedError("Parse json data fail");
			}
		}
		if (respond.ack == 0 ||
			(respond.ack >= 200 && respond.ack < 400)) {
			// ok
			deserialize(doc, out);
			return;
		} else {
			// http error
			throw HttpError::serializeError(doc);
		}
	}

	template<typename T>
	static inline void request(CurlCommand::Command _cmd,
		const std::string &url,
		const std::string &data,
		const std::string &etag,
		T &out) {
		CurlCommand cmd(_cmd, url, data, etag);
		request(cmd, out);
	}
};
