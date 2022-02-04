/*
 * Copyright 2008-2022 Max Kellermann <max.kellermann@gmail.com>
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

#pragma once

#include "Headers.hxx"

#include <curl/curl.h>

#include <cstddef>

struct StringView;
class CurlEasy;
class CurlResponseHandler;

class CurlResponseHandlerAdapter {
	CURL *curl;

	CurlResponseHandler &handler;

	Curl::Headers headers;

	/** error message provided by libcurl */
	char error_buffer[CURL_ERROR_SIZE];

	enum class State {
		UNINITIALISED,
		HEADERS,
		BODY,
		CLOSED,
	} state = State::UNINITIALISED;

public:
	explicit CurlResponseHandlerAdapter(CurlResponseHandler &_handler) noexcept
		:handler(_handler) {}

	void Install(CurlEasy &easy);

	void Done(CURLcode result) noexcept;

private:
	void FinishHeaders();
	void FinishBody();

	void HeaderFunction(StringView s) noexcept;

	/** called by curl when a new header is available */
	static std::size_t _HeaderFunction(char *ptr,
					   std::size_t size, std::size_t nmemb,
					   void *stream) noexcept;

	std::size_t DataReceived(const void *ptr, std::size_t size) noexcept;

	/** called by curl when new data is available */
	static std::size_t WriteFunction(char *ptr,
					 std::size_t size, std::size_t nmemb,
					 void *stream) noexcept;
};
