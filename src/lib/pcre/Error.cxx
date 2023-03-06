// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Error.hxx"

#include <pcre2.h>

namespace Pcre {

ErrorCategory error_category;

std::string
ErrorCategory::message(int condition) const
{
	PCRE2_UCHAR8 buffer[256];
	pcre2_get_error_message_8(condition, buffer, std::size(buffer));
	return std::string{(const char *)buffer};
}

} // namespace Pcre
