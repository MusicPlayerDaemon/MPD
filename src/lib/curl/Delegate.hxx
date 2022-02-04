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
	void OnData(ConstBuffer<void> data) final;
	void OnEnd() final;
};
