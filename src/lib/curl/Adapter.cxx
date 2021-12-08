/*
 * Copyright 2008-2021 Max Kellermann <max.kellermann@gmail.com>
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

#include "Adapter.hxx"
#include "Easy.hxx"
#include "Handler.hxx"
#include "util/CharUtil.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringStrip.hxx"
#include "util/StringView.hxx"

#include <algorithm>
#include <cassert>

void
CurlResponseHandlerAdapter::Install(CurlEasy &easy)
{
	assert(state == State::UNINITIALISED);

	error_buffer[0] = 0;
	easy.SetErrorBuffer(error_buffer);

	easy.SetHeaderFunction(_HeaderFunction, this);
	easy.SetWriteFunction(WriteFunction, this);

	curl = easy.Get();

	state = State::HEADERS;
}

void
CurlResponseHandlerAdapter::FinishHeaders()
{
	assert(state >= State::HEADERS);

	if (state != State::HEADERS)
		return;

	state = State::BODY;

	long status = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

	handler.OnHeaders(status, std::move(headers));
}

void
CurlResponseHandlerAdapter::FinishBody()
{
	FinishHeaders();

	if (state != State::BODY)
		return;

	state = State::CLOSED;
	handler.OnEnd();
}

void
CurlResponseHandlerAdapter::Done(CURLcode result) noexcept
{
	try {
		if (result != CURLE_OK) {
			StripRight(error_buffer);
			const char *msg = error_buffer;
			if (*msg == 0)
				msg = curl_easy_strerror(result);
			throw FormatRuntimeError("CURL failed: %s", msg);
		}

		FinishBody();
	} catch (...) {
		state = State::CLOSED;
		handler.OnError(std::current_exception());
	}
}

[[gnu::pure]]
static bool
IsResponseBoundaryHeader(StringView s) noexcept
{
	return s.size > 5 && (s.StartsWith("HTTP/") ||
			      /* the proprietary "ICY 200 OK" is
				 emitted by Shoutcast */
			      s.StartsWith("ICY 2"));
}

inline void
CurlResponseHandlerAdapter::HeaderFunction(StringView s) noexcept
{
	if (state > State::HEADERS)
		return;

	if (IsResponseBoundaryHeader(s)) {
		/* this is the boundary to a new response, for example
		   after a redirect */
		headers.clear();
		return;
	}

	const char *header = s.data;
	const char *end = StripRight(header, header + s.size);

	const char *value = s.Find(':');
	if (value == nullptr)
		return;

	std::string name(header, value);
	std::transform(name.begin(), name.end(), name.begin(),
		       static_cast<char(*)(char)>(ToLowerASCII));

	/* skip the colon */

	++value;

	/* strip the value */

	value = StripLeft(value, end);
	end = StripRight(value, end);

	headers.emplace(std::move(name), std::string(value, end));
}

std::size_t
CurlResponseHandlerAdapter::_HeaderFunction(char *ptr, std::size_t size,
					    std::size_t nmemb,
					    void *stream) noexcept
{
	CurlResponseHandlerAdapter &c = *(CurlResponseHandlerAdapter *)stream;

	size *= nmemb;

	c.HeaderFunction({ptr, size});
	return size;
}

inline std::size_t
CurlResponseHandlerAdapter::DataReceived(const void *ptr,
					 std::size_t received_size) noexcept
{
	assert(received_size > 0);

	try {
		FinishHeaders();
		handler.OnData({ptr, received_size});
		return received_size;
	} catch (CurlResponseHandler::Pause) {
		return CURL_WRITEFUNC_PAUSE;
	}

}

std::size_t
CurlResponseHandlerAdapter::WriteFunction(char *ptr, std::size_t size,
					  std::size_t nmemb,
					  void *stream) noexcept
{
	CurlResponseHandlerAdapter &c = *(CurlResponseHandlerAdapter *)stream;

	size *= nmemb;
	if (size == 0)
		return 0;

	return c.DataReceived(ptr, size);
}
