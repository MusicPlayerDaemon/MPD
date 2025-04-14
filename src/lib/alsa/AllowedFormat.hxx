// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "pcm/AudioFormat.hxx"
#include "pcm/Features.h" // for ENABLE_DSD

#include <forward_list>
#include <string>

namespace Alsa {

/**
 * An audio format for the "allowed_formats" setting of
 * #AlsaOutputPlugin.
 */
struct AllowedFormat {
	AudioFormat format;
#ifdef ENABLE_DSD
	bool dop;
#endif

	/**
	 * Parse a format string.
	 *
	 * Throws std::runtime_error on error.
	 */
	explicit AllowedFormat(std::string_view s);

	/**
	 * Parse a list of formats separated by space.
	 *
	 * Throws std::runtime_error on error.
	 */
	static std::forward_list<AllowedFormat> ParseList(std::string_view s);
};

std::string
ToString(const std::forward_list<AllowedFormat> &allowed_formats) noexcept;

} // namespace Alsa
