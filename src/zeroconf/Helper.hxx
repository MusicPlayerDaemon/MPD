// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_ZEROCONF_HELPER_HXX
#define MPD_ZEROCONF_HELPER_HXX

#include "config.h"

#include <memory>

class EventLoop;
class AvahiHelper;
class BonjourHelper;

class ZeroconfHelper final {
#ifdef HAVE_AVAHI
	std::unique_ptr<AvahiHelper> helper;
#endif

#ifdef HAVE_BONJOUR
	std::unique_ptr<BonjourHelper> helper;
#endif

public:
	ZeroconfHelper(EventLoop &event_loop, const char *name,
		       const char *service_type, unsigned port);

	~ZeroconfHelper() noexcept;
};

#endif
