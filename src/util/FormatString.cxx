// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "FormatString.hxx"
#include "AllocatedString.hxx"

#include <stdio.h>
#include <stdlib.h>

AllocatedString
FormatStringV(const char *fmt, std::va_list args) noexcept
{
	std::va_list tmp;
	va_copy(tmp, args);
	const int length = vsnprintf(nullptr, 0, fmt, tmp);
	va_end(tmp);

	if (length <= 0)
		/* wtf.. */
		abort();

	char *buffer = new char[length + 1];
	vsnprintf(buffer, length + 1, fmt, args);
	return AllocatedString::Donate(buffer);
}

AllocatedString
FormatString(const char *fmt, ...) noexcept
{
	std::va_list args;
	va_start(args, fmt);
	auto p = FormatStringV(fmt, args);
	va_end(args);
	return p;
}
