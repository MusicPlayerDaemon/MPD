// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "Headers.hxx"

#include <curl/curl.h>

#include <cstddef>
#include <exception>
#include <string_view>

class CurlEasy;
class CurlResponseHandler;

class CurlResponseHandlerAdapter {
	CURL *curl;

	CurlResponseHandler &handler;

	Curl::Headers headers;

	/**
	 * An exception caught from within the WriteFunction() which
	 * will later be handled by Done().
	 */
	std::exception_ptr postponed_error;

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

	void HeaderFunction(std::string_view s) noexcept;

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
