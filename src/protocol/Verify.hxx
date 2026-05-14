// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <string_view>

class Path;

/**
 * Is this a valid string for transmitting it as a value over the MPD
 * text protocol?
 */
[[gnu::pure]]
bool
VerifyStringUTF8(const char *s) noexcept;

[[gnu::pure]]
bool
VerifyStringUTF8(std::string_view s) noexcept;

/**
 * Is this a valid filename (for transmitting it as a value over the
 * MPD text protocol)?
 *
 * Note: this function is designed to be used to verify actual file
 * names (that were actually seen); for example, it does not check for
 * path separators which would be invalid in a bare file name.
 */
[[gnu::pure]]
static inline
bool
VerifySeenFilenameUTF8(const char *filename_utf8) noexcept
{
	return VerifyStringUTF8(filename_utf8);
}

[[gnu::pure]]
bool
VerifySeenFilename(Path filename_fs) noexcept;

/**
 * Is this a valid path (for transmitting it as a value over the MPD
 * text protocol)?
 */
[[gnu::pure]]
bool
VerifyPathUTF8(std::string_view path_utf8) noexcept;

/**
 * Is this a valid relative path (for transmitting it as a value over
 * the MPD text protocol)?
 */
[[gnu::pure]]
bool
VerifyRelativePathUTF8(std::string_view path_utf8) noexcept;

/**
 * Is this a valid URI string (for transmitting it as a value over
 * the MPD text protocol)?
 *
 * In the MPD protocol, URIs can be actual URIs or absolute/relative
 * file paths.  This function allows all of these.
 */
[[gnu::pure]]
bool
VerifyUriUTF8(std::string_view uri_utf8) noexcept;
