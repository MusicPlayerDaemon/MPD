// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Message.hxx"
#include "util/CharUtil.hxx"

static constexpr bool
valid_channel_char(const char ch) noexcept
{
	return IsAlphaNumericASCII(ch) ||
		ch == '_' || ch == '-' || ch == '.' || ch == ':';
}

bool
client_message_valid_channel_name(const char *name) noexcept
{
	do {
		if (!valid_channel_char(*name))
			return false;
	} while (*++name != 0);

	return true;
}
