// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef QOBUZ_SESSION_HXX
#define QOBUZ_SESSION_HXX

#include <string>

class CurlRequest;

struct QobuzSession {
	std::string user_auth_token, device_id;

	bool IsDefined() const noexcept {
		return !user_auth_token.empty();
	}

	void Clear() {
		user_auth_token.clear();
		device_id.clear();
	}
};

#endif
