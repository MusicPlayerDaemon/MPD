// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef QOBUZ_ERROR_PARSER_HXX
#define QOBUZ_ERROR_PARSER_HXX

#include "lib/curl/Headers.hxx"
#include "lib/yajl/ResponseParser.hxx"

#include <string_view>

/**
 * Parse an error JSON response.
 */
class QobuzErrorParser final : public YajlResponseParser {
	const unsigned status;

	enum class State {
		NONE,
		MESSAGE,
	} state = State::NONE;

	std::string message;

public:
	/**
	 * May throw if there is a formal error in the response
	 * headers.
	 */
	QobuzErrorParser(unsigned status, const Curl::Headers &headers);

protected:
	/* virtual methods from CurlResponseParser */
	[[noreturn]]
	void OnEnd() override;

public:
	/* yajl callbacks */
	bool String(std::string_view value) noexcept;
	bool MapKey(std::string_view value) noexcept;
	bool EndMap() noexcept;
};

#endif
