// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Path.hxx"
#include "Charset.hxx"

std::string
Path::ToUTF8() const noexcept
{
	try {
		return ToUTF8Throw();
	} catch (...) {
		return {};
	}
}

std::string
Path::ToUTF8Throw() const
{
	return ::PathToUTF8(c_str());
}

Path::const_pointer
Path::GetSuffix() const noexcept
{
	const auto *base = GetBase().c_str();

	/* skip all leading dots (hidden/special files on UNIX-like
	   operating systems) */
	while (*base == '.')
		++base;

	return StringFindLast(base, '.');
}

Path::const_pointer
Path::GetExtension() const noexcept
{
	const auto *result = GetSuffix();
	if (result != nullptr)
		/* skip the dot */
		++result;

	return result;
}
