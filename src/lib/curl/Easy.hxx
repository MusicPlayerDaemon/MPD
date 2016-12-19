/*
 * Copyright (C) 2016 Max Kellermann <max@duempel.org>
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

#ifndef CURL_EASY_HXX
#define CURL_EASY_HXX

#include <curl/curl.h>

#include <utility>
#include <stdexcept>
#include <cstddef>

/**
 * An OO wrapper for a "CURL*" (a libCURL "easy" handle).
 */
class CurlEasy {
	CURL *handle = nullptr;

public:
	/**
	 * Allocate a new CURL*.
	 *
	 * Throws std::runtime_error on error.
	 */
	CurlEasy()
		:handle(curl_easy_init())
	{
		if (handle == nullptr)
			throw std::runtime_error("curl_easy_init() failed");
	}

	/**
	 * Create an empty instance.
	 */
	CurlEasy(std::nullptr_t):handle(nullptr) {}

	CurlEasy(CurlEasy &&src):handle(std::exchange(src.handle, nullptr)) {}

	~CurlEasy() {
		if (handle != nullptr)
			curl_easy_cleanup(handle);
	}

	operator bool() const {
		return handle != nullptr;
	}

	CurlEasy &operator=(CurlEasy &&src) {
		std::swap(handle, src.handle);
		return *this;
	}

	CURL *Get() {
		return handle;
	}

	template<typename T>
	void SetOption(CURLoption option, T value) {
		CURLcode code = curl_easy_setopt(handle, option, value);
		if (code != CURLE_OK)
			throw std::runtime_error(curl_easy_strerror(code));
	}
};

#endif
