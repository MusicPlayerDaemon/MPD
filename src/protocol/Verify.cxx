// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Verify.hxx"
#include "fs/Path.hxx"
#include "util/UTF8.hxx"

#include <cstring> // for std::strchr()

/**
 * Is this a valid string for transmitting it as a value over the MPD
 * text protocol?
 */
bool
VerifyStringUTF8(const char *s) noexcept
{
	/* newlines cannot be represented in MPD's protocol */
	return std::strchr(s, '\n') == nullptr &&
		/* the MPD protocol is UTF-8 only */
		ValidateUTF8(s);
}

bool
VerifyStringUTF8(std::string_view s) noexcept
{
	/* newlines cannot be represented in MPD's protocol */
	return s.find('\n') == s.npos &&
		/* null bytes are forbidden, too */
		s.find('\0') == s.npos &&
		/* the MPD protocol is UTF-8 only */
		ValidateUTF8(s);
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
	if (!VerifyPathUTF8(path_utf8) || path_utf8.front() == '/')
		return false;

	while (!path_utf8.empty()) {
		auto slash = path_utf8.find('/');
		auto component = slash == path_utf8.npos
			? path_utf8
			: path_utf8.substr(0, slash);

		if (component.empty() || component == "." || component == "..")
			return false;

		if (slash == path_utf8.npos)
			break;

		path_utf8.remove_prefix(slash + 1);
	}

	return true;
}

bool
VerifyUriUTF8(std::string_view uri_utf8) noexcept
{
	return !uri_utf8.empty() && VerifyStringUTF8(uri_utf8);
}
