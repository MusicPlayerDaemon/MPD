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

#include "Helper.hxx"
#include "Client.hxx"
#include "ErrorHandler.hxx"
#include "Publisher.hxx"
#include "Service.hxx"
#include "util/RuntimeError.hxx"
#include "Log.hxx"

#include <avahi-common/domain.h>

class SharedAvahiClient final : public Avahi::ErrorHandler {
public:
	Avahi::Client client;

	SharedAvahiClient(EventLoop &event_loop)
		:client(event_loop, *this) {}

	/* virtual methods from class Avahi::ErrorHandler */
	bool OnAvahiError(std::exception_ptr e) noexcept override {
		LogError(e);
		return true;
	}
};

static std::weak_ptr<SharedAvahiClient> shared_avahi_client;

inline
AvahiHelper::AvahiHelper(std::shared_ptr<SharedAvahiClient> _client,
			 std::unique_ptr<Avahi::Publisher> _publisher)
	:client(std::move(_client)),
	 publisher(std::move(_publisher)) {}

AvahiHelper::~AvahiHelper() noexcept = default;

std::unique_ptr<AvahiHelper>
AvahiInit(EventLoop &event_loop, const char *service_name,
	  const char *service_type, unsigned port)
{
	if (!avahi_is_valid_service_name(service_name))
		throw FormatRuntimeError("Invalid zeroconf_name \"%s\"",
					 service_name);

	auto client = shared_avahi_client.lock();
	if (!client)
		shared_avahi_client = client =
			std::make_shared<SharedAvahiClient>(event_loop);

	std::forward_list<Avahi::Service> services;
	services.emplace_front(AVAHI_IF_UNSPEC,
			       AVAHI_PROTO_UNSPEC,
			       service_type, port);

	auto publisher = std::make_unique<Avahi::Publisher>(client->client,
							    service_name,
							    std::move(services),
							    *client);

	return std::make_unique<AvahiHelper>(std::move(client),
					     std::move(publisher));
}
