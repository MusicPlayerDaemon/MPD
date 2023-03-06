// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "Handler.hxx"

#include <memory>

class CurlResponseParser;

/**
 * A #CurlResponseHandler implementation which delegates response body
 * parsing to another object which is created dynamically.  This is
 * useful when a different parser needs to be used under certain
 * runtime conditions (e.g. depending on the status or content type).
 */
class DelegateCurlResponseHandler : public CurlResponseHandler {
	std::unique_ptr<CurlResponseParser> parser;

protected:
	/**
	 * HTTP response headers have been received, and we now need a
	 * parser.  This method constructs one and returns it (or
	 * throws an exception which will be passed to
	 * CurlResponseParser::OnError()).
	 */
	virtual std::unique_ptr<CurlResponseParser> MakeParser(unsigned status,
							       Curl::Headers &&headers) = 0;

	/**
	 * The parser has finished parsing the response body.  This
	 * method can be used to evaluate the result.  Exceptions
	 * thrown by this method will be passed to
	 * CurlResponseParser::OnError().
	 */
	virtual void FinishParser(std::unique_ptr<CurlResponseParser> p) = 0;

public:
	void OnHeaders(unsigned status, Curl::Headers &&headers) final;
	void OnData(std::span<const std::byte> data) final;
	void OnEnd() final;
};
