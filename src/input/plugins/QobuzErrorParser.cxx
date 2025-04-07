// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "QobuzErrorParser.hxx"
#include "lib/curl/StringResponse.hxx"
#include "lib/fmt/RuntimeError.hxx"

#include <nlohmann/json.hpp>

using std::string_view_literals::operator""sv;

void
ThrowQobuzError(const StringCurlResponse &response)
{
	if (auto i = response.headers.find("content-type");
	    i != response.headers.end() && i->second.find("/json") != i->second.npos) {
		const auto json = nlohmann::json::parse(response.body);
		if (auto m = json.find("message"sv); m != json.end())
			throw FmtRuntimeError("Error from Qobuz: {}", m->get<std::string_view>());
	}

	throw FmtRuntimeError("Status {} from Qobuz", response.status);
}

