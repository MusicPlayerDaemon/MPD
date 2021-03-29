/*
 * Copyright 2016-2020 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef CURL_MULTI_HXX
#define CURL_MULTI_HXX

#include <curl/curl.h>

#include <chrono>
#include <utility>
#include <stdexcept>
#include <cstddef>

/**
 * An OO wrapper for a "CURLM*" (a libCURL "multi" handle).
 */
class CurlMulti {
	CURLM *handle = nullptr;

public:
	/**
	 * Allocate a new CURLM*.
	 *
	 * Throws on error.
	 */
	CurlMulti()
		:handle(curl_multi_init())
	{
		if (handle == nullptr)
			throw std::runtime_error("curl_multi_init() failed");
	}

	/**
	 * Create an empty instance.
	 */
	CurlMulti(std::nullptr_t) noexcept:handle(nullptr) {}

	CurlMulti(CurlMulti &&src) noexcept
		:handle(std::exchange(src.handle, nullptr)) {}

	~CurlMulti() noexcept {
		if (handle != nullptr)
			curl_multi_cleanup(handle);
	}

	CurlMulti &operator=(CurlMulti &&src) noexcept {
		std::swap(handle, src.handle);
		return *this;
	}

	operator bool() const noexcept {
		return handle != nullptr;
	}

	CURLM *Get() noexcept {
		return handle;
	}

	template<typename T>
	void SetOption(CURLMoption option, T value) {
		auto code = curl_multi_setopt(handle, option, value);
		if (code != CURLM_OK)
			throw std::runtime_error(curl_multi_strerror(code));
	}

	void Add(CURL *easy) {
		auto code = curl_multi_add_handle(handle, easy);
		if (code != CURLM_OK)
			throw std::runtime_error(curl_multi_strerror(code));
	}

	void Remove(CURL *easy) {
		auto code = curl_multi_remove_handle(handle, easy);
		if (code != CURLM_OK)
			throw std::runtime_error(curl_multi_strerror(code));
	}

	CURLMsg *InfoRead() {
		int msgs_in_queue;
		return curl_multi_info_read(handle, &msgs_in_queue);
	}

	unsigned Perform() {
		int running_handles;
		auto code = curl_multi_perform(handle, &running_handles);
		if (code != CURLM_OK)
			throw std::runtime_error(curl_multi_strerror(code));
		return running_handles;
	}

	unsigned Wait(int timeout=-1) {
		int numfds;
		auto code = curl_multi_wait(handle, nullptr, 0, timeout,
					    &numfds);
		if (code != CURLM_OK)
			throw std::runtime_error(curl_multi_strerror(code));
		return numfds;
	}

	unsigned Wait(std::chrono::milliseconds timeout) {
		return Wait(timeout.count());
	}
};

#endif
