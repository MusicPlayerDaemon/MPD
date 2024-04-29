// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "LocalSocketAddress.hxx"

const char *
LocalSocketAddress::GetLocalPath() const noexcept
{
	const auto raw = GetLocalRaw();
	return !raw.empty() &&
		/* must be an absolute path */
		raw.front() == '/' &&
		/* must be null-terminated and there must not be any
		   other null byte */
		raw.find('\0') == raw.size() - 1
		? raw.data()
		: nullptr;
}
