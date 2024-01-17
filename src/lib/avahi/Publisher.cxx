// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Publisher.hxx"
#include "Service.hxx"
#include "Client.hxx"
#include "Error.hxx"
#include "ErrorHandler.hxx"
#include "net/SocketAddress.hxx"

#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/alternative.h>

#include <fmt/core.h>

#include <cassert>

#include <unistd.h>

namespace Avahi {

/**
 * Append the process id to the given prefix string.  This is used as
 * a workaround for an avahi-daemon bug/problem: when a service gets
 * restarted, and then binds to a new port number (e.g. beng-proxy
 * with automatic port assignment), we don't get notified, and so we
 * never query the new port.  By appending the process id to the
 * client name, we ensure that the exiting old process broadcasts
 * AVAHI_BROWSER_REMOVE, and the new process broadcasts
 * AVAHI_BROWSER_NEW.
 */
static std::string
MakePidName(const char *prefix) noexcept
{
	return fmt::format("{}[{}]", prefix, getpid());
}

Publisher::Publisher(Client &_client, const char *_name,
		     ErrorHandler &_error_handler) noexcept
	:error_handler(_error_handler),
	 name(MakePidName(_name)),
	 client(_client),
	 defer_register_services(client.GetEventLoop(),
				 BIND_THIS_METHOD(DeferredRegisterServices))
{
	client.AddListener(*this);
}

Publisher::~Publisher() noexcept
{
	assert(services.empty());

	client.RemoveListener(*this);
}

void
Publisher::AddService(Service &service) noexcept
{
	services.push_back(service);

	if (visible && client.IsConnected())
		defer_register_services.Schedule();
}

void
Publisher::RemoveService(Service &service) noexcept
{
	services.erase(services.iterator_to(service));

	if (visible && client.IsConnected())
		defer_register_services.Schedule();
}

inline void
Publisher::GroupCallback(AvahiEntryGroup *g,
			 AvahiEntryGroupState state) noexcept
try {
	switch (state) {
	case AVAHI_ENTRY_GROUP_ESTABLISHED:
		break;

	case AVAHI_ENTRY_GROUP_COLLISION:
		if (!visible)
			/* meanwhile, HideServices() has been called */
			return;

		/* pick a new name */

		{
			char *new_name = avahi_alternative_service_name(name.c_str());
			name = new_name;
			avahi_free(new_name);
		}

		/* And recreate the services */
		should_reset_group = false;
		RegisterServices(*g);
		break;

	case AVAHI_ENTRY_GROUP_FAILURE:
		throw MakeError(*avahi_entry_group_get_client(g), "Avahi service group failure");

	case AVAHI_ENTRY_GROUP_UNCOMMITED:
	case AVAHI_ENTRY_GROUP_REGISTERING:
		break;
	}
} catch (...) {
	error_handler.OnAvahiError(std::current_exception());
}

void
Publisher::GroupCallback(AvahiEntryGroup *g,
			 AvahiEntryGroupState state,
			 void *userdata) noexcept
{
	auto &publisher = *(Publisher *)userdata;
	publisher.GroupCallback(g, state);
}

static void
AddService(AvahiEntryGroup &group, const Service &service,
	   const char *name)
{
	int error = avahi_entry_group_add_service(&group,
						  service.interface,
						  service.protocol,
						  AvahiPublishFlags(0),
						  name, service.type.c_str(),
						  nullptr, nullptr,
						  service.port, nullptr);
	if (error != AVAHI_OK)
		throw MakeError(error, "Failed to add Avahi service");
}

static void
AddServices(AvahiEntryGroup &group,
	    const IntrusiveList<Service> &services, const char *name)
{
	for (const auto &i : services)
		AddService(group, i, name);
}

void
Publisher::RegisterServices(AvahiEntryGroup &g)
{
	if (should_reset_group) {
		should_reset_group = false;
		avahi_entry_group_reset(&g);
	}

	AddServices(g, services, name.c_str());

	if (!services.empty()) {
		should_reset_group = true;

		if (int error = avahi_entry_group_commit(&g);
		    error != AVAHI_OK)
			throw MakeError(error, "Failed to commit Avahi service group");
	}
}

void
Publisher::RegisterServices(AvahiClient *c)
{
	assert(visible);

	if (!group) {
		assert(!should_reset_group);

		group.reset(avahi_entry_group_new(c, GroupCallback, this));
		if (!group)
			throw MakeError(*c, "Failed to create Avahi service group");
	}

	RegisterServices(*group);
}

void
Publisher::DeferredRegisterServices() noexcept
{
	assert(visible);
	assert(client.IsConnected());

	try {
		RegisterServices(client.GetClient());
	} catch (...) {
		error_handler.OnAvahiError(std::current_exception());
	}
}

void
Publisher::HideServices() noexcept
{
	if (!visible)
		return;

	visible = false;

	defer_register_services.Cancel();

	if (group) {
		should_reset_group = false;
		avahi_entry_group_reset(group.get());
	} else {
		assert(!should_reset_group);
	}
}

void
Publisher::ShowServices() noexcept
{
	if (visible)
		return;

	visible = true;

	if (client.IsConnected())
		defer_register_services.Schedule();
}

void
Publisher::OnAvahiConnect(AvahiClient *c) noexcept
{
	assert(!group);
	assert(!should_reset_group);

	if (visible && !services.empty())
		try {
			RegisterServices(c);
		} catch (...) {
			error_handler.OnAvahiError(std::current_exception());
		}
}

void
Publisher::OnAvahiDisconnect() noexcept
{
	group.reset();
	should_reset_group = false;
}

void
Publisher::OnAvahiChanged() noexcept
{
	group.reset();
	should_reset_group = false;
}

} // namespace Avahi
