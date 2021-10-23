/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPD_ALSA_ALLOWED_FORMAT_HXX
#define MPD_ALSA_ALLOWED_FORMAT_HXX

#include "pcm/AudioFormat.hxx"
#include "config.h"

#include <forward_list>
#include <string>

struct StringView;

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
	explicit AllowedFormat(StringView s);

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

#endif
