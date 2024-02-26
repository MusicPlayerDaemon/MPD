// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <curl/curl.h>

#include <utility>

/**
 * An OO wrapper for an allocated string to be freed with curl_free().
 */
class CurlString {
	char *p = nullptr;

public:
	CurlString() noexcept = default;
	CurlString(std::nullptr_t) noexcept {}

	explicit CurlString(char *_p) noexcept
		:p(_p) {}

	CurlString(CurlString &&src) noexcept
		:p(std::exchange(src.p, nullptr)) {}

	~CurlString() noexcept {
		if (p != nullptr)
			curl_free(p);
	}

	CurlString &operator=(CurlString &&src) noexcept {
		using std::swap;
		swap(p, src.p);
		return *this;
	}

	operator bool() const noexcept {
		return p != nullptr;
	}

	operator const char *() const noexcept {
		return p;
	}

	const char *c_str() const noexcept {
		return p;
	}
};
