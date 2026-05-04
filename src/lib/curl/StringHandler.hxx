// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Handler.hxx"
#include "StringOptions.hxx"
#include "StringResponse.hxx"

namespace Curl {

/**
 * A #CurlResponseHandler implementation which stores the response
 * body in a std::string.
 */
class StringResponseHandler : public CurlResponseHandler {
	const StringOptions options;

	StringResponse response;

	std::exception_ptr error;

public:
	[[nodiscard]]
	StringResponseHandler() noexcept = default;

	[[nodiscard]]
	explicit StringResponseHandler(StringOptions &_options) noexcept
		:options(_options) {}

	void CheckRethrowError() const {
		if (error)
			std::rethrow_exception(error);
	}

	const StringResponse &GetResponse() const {
		CheckRethrowError();

		return std::move(response);
	}

	StringResponse TakeResponse() && {
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

} // namespace Curl
