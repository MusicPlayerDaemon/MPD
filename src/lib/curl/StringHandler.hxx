// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Handler.hxx"
#include "StringResponse.hxx"

/**
 * A #CurlResponseHandler implementation which stores the response
 * body in a std::string.
 */
class StringCurlResponseHandler : public CurlResponseHandler {
	StringCurlResponse response;

	std::exception_ptr error;

public:
	void CheckRethrowError() const {
		if (error)
			std::rethrow_exception(error);
	}

	const StringCurlResponse &GetResponse() const {
		CheckRethrowError();

		return std::move(response);
	}

	StringCurlResponse TakeResponse() && {
		CheckRethrowError();

		return std::move(response);
	}

public:
	/* virtual methods from class CurlResponseHandler */
	void OnHeaders(unsigned status, Curl::Headers &&headers) override;
	void OnData(std::span<const std::byte> data) override;
	void OnEnd() override;
	void OnError(std::exception_ptr e) noexcept override;
};
