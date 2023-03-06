// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "Handle.hxx"
#include "lib/curl/Parser.hxx"

/**
 * A helper class which parses the response body as JSON.
 */
class YajlResponseParser : public CurlResponseParser {
	Yajl::Handle handle;

public:
	template<typename... Args>
	YajlResponseParser(Args... args) noexcept
		:handle(std::forward<Args>(args)...) {}

	/* virtual methods fro CurlResponseParser */
	void OnData(std::span<const std::byte> data) final;
	void OnEnd() override;
};
