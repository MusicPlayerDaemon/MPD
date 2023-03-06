// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <exception>

struct AvahiClient;

namespace Avahi {

class ErrorHandler {
public:
	/**
	 * @return true to keep retrying, false if the failed object
	 * has been disposed
	 */
	virtual bool OnAvahiError(std::exception_ptr e) noexcept = 0;
};

} // namespace Avahi
