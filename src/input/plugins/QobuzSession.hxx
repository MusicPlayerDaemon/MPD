// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <string>

class CurlRequest;

struct QobuzSession {
	std::string user_auth_token;

	bool IsDefined() const noexcept {
		return !user_auth_token.empty();
	}
};
