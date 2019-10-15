/*
 * Copyright 2019 Max Kellermann <max.kellermann@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CURL_STRING_HXX
#define CURL_STRING_HXX

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

#endif
