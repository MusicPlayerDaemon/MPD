// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <memory>

class EventLoop;
class AvahiHelper;

class ZeroconfHelper final {
	std::unique_ptr<AvahiHelper> helper;

public:
	ZeroconfHelper(EventLoop &event_loop, const char *name,
		       const char *service_type, unsigned port);

	~ZeroconfHelper() noexcept;
};
