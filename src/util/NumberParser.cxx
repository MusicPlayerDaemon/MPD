// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "NumberParser.hxx"

#include <algorithm>
#include <iterator>

int64_t
ParseInt64(std::string_view s, const char **endptr_r, int base) noexcept
{
	char buffer[32];
	*std::copy_n(s.data(), std::min(s.size(), std::size(buffer) - 1),
		     buffer) = 0;

	char *endptr;
	const auto result = ParseInt64(buffer, &endptr, base);
	if (endptr_r != nullptr)
		*endptr_r = s.data() + (endptr - buffer);

	return result;
}
