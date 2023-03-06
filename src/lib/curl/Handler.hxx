// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "Headers.hxx"

#include <cstddef>
#include <exception>
#include <span>

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
	 *
	 * Exceptions thrown by this method will be passed to
	 * OnError(), aborting the request.
	 */
	virtual void OnHeaders(unsigned status, Curl::Headers &&headers) = 0;

	/**
	 * Response body data has been received.
	 *
	 * May throw #Pause.
	 *
	 * Other exceptions thrown by this method will be passed to
	 * OnError(), aborting the request.
	 */
	virtual void OnData(std::span<const std::byte> data) = 0;

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
