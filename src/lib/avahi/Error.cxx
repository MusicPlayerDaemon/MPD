// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Error.hxx"

#include <avahi-client/client.h>
#include <avahi-common/error.h>

#include <system_error>

namespace Avahi {

ErrorCategory error_category;

std::string
ErrorCategory::message(int condition) const
{
	return avahi_strerror(condition);
}

std::system_error
MakeError(AvahiClient &client, const char *msg) noexcept
{
	return MakeError(avahi_client_errno(&client), msg);
}

} // namespace Avahi
