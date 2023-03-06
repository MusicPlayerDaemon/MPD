// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Handle.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "util/ScopeExit.hxx"
#include "util/StringStrip.hxx"

#include <cstring>

/**
 * Strip whitespace at the beginning and end and replace newline
 * characters which are illegal in the MPD protocol.
 */
static const char *
StripErrorMessage(char *s) noexcept
{
	s = Strip(s);

	while (auto newline = std::strchr(s, '\n'))
		*newline = ';';

	return s;
}

namespace Yajl {

void
Handle::ThrowError()
{
	unsigned char *str = yajl_get_error(handle, false,
					    nullptr, 0);
	AtScopeExit(this, str) {
		yajl_free_error(handle, str);
	};

	throw FmtRuntimeError("Failed to parse JSON: {}",
			      StripErrorMessage((char *)str));
}

} // namespace Yajl
