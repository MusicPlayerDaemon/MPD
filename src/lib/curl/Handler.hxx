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

#pragma once

#include "Headers.hxx"
#include "util/ConstBuffer.hxx"

#include <exception>

/**
 * Asynchronous response handler for a #CurlRequest.
 *
 * Its methods must be thread-safe.
 */
class CurlResponseHandler {
public:
	/**
	 * OnData() shall throw this to pause the stream.  Call
	 * CurlEasy::Unpause() or CurlRequest::Resume() to resume the
	 * transfer.
	 */
	struct Pause {};

	/**
	 * Status line and headers have been received.
	 */
	virtual void OnHeaders(unsigned status, Curl::Headers &&headers) = 0;

	/**
	 * Response body data has been received.
	 *
	 * May throw #Pause (but nothing else).
	 */
	virtual void OnData(ConstBuffer<void> data) = 0;

	/**
	 * The response has ended.  The method is allowed to delete the
	 * #CurlRequest.
	 */
	virtual void OnEnd() = 0;

	/**
	 * An error has occurred.  The method is allowed to delete the
	 * #CurlRequest.
	 */
	virtual void OnError(std::exception_ptr e) noexcept = 0;
};
