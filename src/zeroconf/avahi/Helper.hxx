// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "lib/avahi/Publisher.hxx"
#include "lib/avahi/Service.hxx"

#include <memory>

class EventLoop;
namespace Avahi { class Publisher; }

class SharedAvahiClient;

class AvahiHelper final {
	std::shared_ptr<SharedAvahiClient> client;
	Avahi::Publisher publisher;
	Avahi::Service service;

public:
	AvahiHelper(std::shared_ptr<SharedAvahiClient> _client,
		    const char *service_name,
		    const char *service_type, unsigned port);
	~AvahiHelper() noexcept;
};

std::unique_ptr<AvahiHelper>
AvahiInit(EventLoop &event_loop, const char *service_name,
	  const char *service_type, unsigned port);
