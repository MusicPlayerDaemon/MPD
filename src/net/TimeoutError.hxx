// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <stdexcept>

/**
 * Some operation has timed out (e.g. connecting to a server, waiting
 * for reply from a server).
 */
class TimeoutError : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;

	TimeoutError() noexcept
		:std::runtime_error("Timeout") {}
};
