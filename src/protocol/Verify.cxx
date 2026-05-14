// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Verify.hxx"
#include "fs/Path.hxx"

#include <cstring> // for std::strchr()

/**
 * Is this a valid string for transmitting it as a value over the MPD
 * text protocol?
 */
bool
VerifyStringUTF8(const char *s) noexcept
{
	/* newlines cannot be represented in MPD's protocol */
	return std::strchr(s, '\n') == nullptr;
}

bool
VerifyStringUTF8(std::string_view s) noexcept
{
	/* newlines cannot be represented in MPD's protocol */
	return s.find('\n') == s.npos;
}

bool
VerifySeenFilename(Path filename_fs) noexcept
{
	return !filename_fs.HasNewline();
}

bool
VerifyPathUTF8(std::string_view path_utf8) noexcept
{
	return !path_utf8.empty() && VerifyStringUTF8(path_utf8);
}

bool
VerifyRelativePathUTF8(std::string_view path_utf8) noexcept
{
	// TODO check whether it's a relative path
	return VerifyPathUTF8(path_utf8);
}

bool
VerifyUriUTF8(std::string_view uri_utf8) noexcept
{
	return !uri_utf8.empty() && VerifyStringUTF8(uri_utf8);
}
