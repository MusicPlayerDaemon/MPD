// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_ZEROCONF_AVAHI_HELPER_HXX
#define MPD_ZEROCONF_AVAHI_HELPER_HXX

#include <memory>

class EventLoop;
namespace Avahi { class Publisher; }

class SharedAvahiClient;

class AvahiHelper final {
	std::shared_ptr<SharedAvahiClient> client;
	std::unique_ptr<Avahi::Publisher> publisher;

public:
	AvahiHelper(std::shared_ptr<SharedAvahiClient> _client,
		    std::unique_ptr<Avahi::Publisher> _publisher);
	~AvahiHelper() noexcept;
};

std::unique_ptr<AvahiHelper>
AvahiInit(EventLoop &event_loop, const char *service_name,
	  const char *service_type, unsigned port);

#endif
