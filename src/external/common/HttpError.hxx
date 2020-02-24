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

#include "Compiler.h"
#include "IJson.hxx"
#include "external/jaijson/jaijson.hxx"

#include <stdexcept>

enum class HttpResult {
	HTTP_Bad_request		= 400,
	HTTP_Unauthorized		= 401,
	HTTP_Forbidden 			= 403,
	HTTP_Not_found			= 404,
	HTTP_Too_many_requests	= 429,
	HTTP_Unexpected_error	= 500,
	HTTP_Bad_gateway		= 502,
	HTTP_Service_unavailable= 503,
	HTTP_Request_timeout	= 504,
};

class HttpError : public std::runtime_error {
	HttpResult code = (HttpResult)0;

public:
	HttpError(HttpResult _code, const char *msg)
		:std::runtime_error(msg), code(_code) {}

	HttpResult getCode() const {
		return code;
	}

	static HttpError codeError(int c);

	static HttpError serializeError(const jaijson::Value &d);

	static HttpError badRequest() {
		return HttpError(HttpResult::HTTP_Bad_request,
				     "Bad request");
	}

	static HttpError unauthorized() {
		return HttpError(HttpResult::HTTP_Unauthorized,
				     "Unauthorized");
	}

	static HttpError format(HttpResult c, const char *fmt, ...);

	static HttpError forbidden() {
		return HttpError(HttpResult::HTTP_Forbidden,
					 "Forbidden");
	}

	static HttpError notFound() {
		return HttpError(HttpResult::HTTP_Not_found,
					 "Not found");
	}

	static HttpError tooManyRequests() {
		return HttpError(HttpResult::HTTP_Too_many_requests,
					 "Too many requests");
	}

	static HttpError unexpectedError() {
		return unexpectedError("Unexpected error");
	}

	static HttpError unexpectedError(const char *fmt, ...);

	static HttpError badGateway() {
		return HttpError(HttpResult::HTTP_Bad_gateway,
					 "Bad gateway");
	}

	static HttpError serviceUnavailable() {
		return HttpError(HttpResult::HTTP_Service_unavailable,
					 "Service unavailable");
	}

	static HttpError requestTimeout() {
		return HttpError(HttpResult::HTTP_Request_timeout,
					 "Request timeout");
	}
};
