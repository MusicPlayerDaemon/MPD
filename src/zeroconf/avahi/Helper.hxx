/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

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
