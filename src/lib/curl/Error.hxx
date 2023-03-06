// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <curl/curl.h>

#include <system_error>

namespace Curl {

class ErrorCategory final : public std::error_category {
public:
	const char *name() const noexcept override {
		return "curl";
	}

	std::string message(int condition) const override {
		return curl_easy_strerror(static_cast<CURLcode>(condition));
	}
};

inline ErrorCategory error_category;

inline std::system_error
MakeError(CURLcode code, const char *msg) noexcept
{
	return std::system_error(static_cast<int>(code), error_category, msg);
}

} // namespace Curl
