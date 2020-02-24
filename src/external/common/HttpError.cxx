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

#include "config.h"
#include "HttpError.hxx"
#include "external/jaijson/Deserializer.hxx"
#include "external/jaijson/Serializer.hxx"

#include <stdarg.h>
#include <stdio.h>

HttpError
HttpError::codeError(int c)
{
	switch ((HttpResult)c) {
	case HttpResult::HTTP_Bad_request:
		return badRequest();
	case HttpResult::HTTP_Unauthorized:
		return unauthorized();
	case HttpResult::HTTP_Forbidden:
		return forbidden();
	case HttpResult::HTTP_Not_found:
		return notFound();
	case HttpResult::HTTP_Too_many_requests:
		return tooManyRequests();
	case HttpResult::HTTP_Unexpected_error:
		return unexpectedError();
	case HttpResult::HTTP_Bad_gateway:
		return badGateway();
	case HttpResult::HTTP_Service_unavailable:
		return serviceUnavailable();
	case HttpResult::HTTP_Request_timeout:
		return requestTimeout();
	default:
		return unexpectedError();
	}
}

static bool
tidal_serialize(const rapidjson::Value &d, int &code, std::string &msg)
{
	using namespace jaijson;
	code = 0;
	deserialize(d, "status", code);
	deserialize(d, "userMessage", msg);

	return code != 0;
}

static bool
spotify_serialize(const rapidjson::Value &v, int &code, std::string &msg)
{
	using namespace jaijson;
	code = 0;
	const auto _error = v.FindMember("error");
	if (_error != v.MemberEnd() && _error->value.IsObject()) {
		deserialize(_error->value, "status", code);
		deserialize(_error->value, "message", msg);
	} else if (_error != v.MemberEnd()
		&& _error->value.IsString()) {
		code = (int)HttpResult::HTTP_Unexpected_error;
		msg = _error->value.GetString();
	} else {
		code = 0;
		msg.clear();
	}

	if (msg.empty()) {
		deserialize(v, "error_description", msg);
		if (!msg.empty()) {
			code = (int)HttpResult::HTTP_Unauthorized;
		}
	}

	return code != 0;
}

HttpError
HttpError::serializeError(const jaijson::Value &v)
{
	int code;
	std::string msg;

	if (tidal_serialize(v, code, msg) ||
		spotify_serialize(v, code, msg)) {
		return HttpError((HttpResult)code, msg.c_str());
	} else {
		return unexpectedError();
	}
}

HttpError
HttpError::format(HttpResult c, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	char msg[1024];
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	return HttpError(c, msg);
}

HttpError
HttpError::unexpectedError(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	char msg[1024];
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	return HttpError(HttpResult::HTTP_Unexpected_error, msg);
}

