// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Helper.hxx"
#include "lib/avahi/Client.hxx"
#include "lib/avahi/ErrorHandler.hxx"
#include "lib/fmt/RuntimeError.hxx"
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
			 const char *service_name,
			 const char *service_type, unsigned port)
	:client(std::move(_client)),
	 publisher(client->client, service_name, *client),
	 service(AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC,
		 service_type, port)
{
	publisher.AddService(service);
}

AvahiHelper::~AvahiHelper() noexcept
{
	publisher.RemoveService(service);
}

std::unique_ptr<AvahiHelper>
AvahiInit(EventLoop &event_loop, const char *service_name,
	  const char *service_type, unsigned port)
{
	if (!avahi_is_valid_service_name(service_name))
		throw FmtRuntimeError("Invalid zeroconf_name \"{}\"",
				      service_name);

	auto client = shared_avahi_client.lock();
	if (!client)
		shared_avahi_client = client =
			std::make_shared<SharedAvahiClient>(event_loop);

	return std::make_unique<AvahiHelper>(std::move(client), service_name,
					     service_type, port);
}
