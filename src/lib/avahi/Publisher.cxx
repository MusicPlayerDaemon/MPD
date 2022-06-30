/*
 * Copyright 2007-2021 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Publisher.hxx"
#include "Service.hxx"
#include "Client.hxx"
#include "Error.hxx"
#include "ErrorHandler.hxx"
#include "net/SocketAddress.hxx"

#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/alternative.h>

#include <cassert>

#include <stdio.h>
#include <unistd.h>

namespace Avahi {

/**
 * Append the process id to the given prefix string.  This is used as
 * a workaround for an avahi-daemon bug/problem: when a service gets
 * restarted, and then binds to a new port number (e.g. beng-proxy
 * with automatic port assignment), we don't get notified, and so we
 * never query the new port.  By appending the process id to the
 * client name, we ensure that the exiting old process broadcasts
 * AVAHI_BROWSER_REMOVE, and hte new process broadcasts
 * AVAHI_BROWSER_NEW.
 */
static std::string
MakePidName(const char *prefix)
{
	char buffer[256];
	snprintf(buffer, sizeof(buffer), "%s[%u]", prefix, (unsigned)getpid());
	return buffer;
}

Publisher::Publisher(Client &_client, const char *_name,
		     std::forward_list<Service> _services,
		     ErrorHandler &_error_handler) noexcept
	:error_handler(_error_handler),
	 name(MakePidName(_name)),
	 client(_client), services(std::move(_services))
{
	assert(!services.empty());

	client.AddListener(*this);

	auto *c = client.GetClient();
	if (c != nullptr)
		RegisterServices(c);
}

Publisher::~Publisher() noexcept
{
	client.RemoveListener(*this);
}

inline void
Publisher::GroupCallback(AvahiEntryGroup *g,
			 AvahiEntryGroupState state) noexcept
{
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
		RegisterServices(avahi_entry_group_get_client(g));
		break;

	case AVAHI_ENTRY_GROUP_FAILURE:
		error_handler.OnAvahiError(std::make_exception_ptr(MakeError(*avahi_entry_group_get_client(g),
									     "Avahi service group failure")));
		break;

	case AVAHI_ENTRY_GROUP_UNCOMMITED:
	case AVAHI_ENTRY_GROUP_REGISTERING:
		break;
	}
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
	    const std::forward_list<Service> &services, const char *name)
{
	for (const auto &i : services)
		AddService(group, i, name);
}

static EntryGroupPtr
MakeEntryGroup(AvahiClient &c,
	       const std::forward_list<Service> &services, const char *name,
	       AvahiEntryGroupCallback callback, void *userdata)
{
	EntryGroupPtr group(avahi_entry_group_new(&c, callback, userdata));
	if (!group)
		throw MakeError(c, "Failed to create Avahi service group");

	AddServices(*group, services, name);

	int error = avahi_entry_group_commit(group.get());
	if (error != AVAHI_OK)
		throw MakeError(error, "Failed to commit Avahi service group");

	return group;
}

void
Publisher::RegisterServices(AvahiClient *c) noexcept
{
	assert(visible);

	try {
		group = MakeEntryGroup(*c, services, name.c_str(),
				       GroupCallback, this);
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
	group.reset();
}

void
Publisher::ShowServices() noexcept
{
	if (visible)
		return;

	visible = true;

	auto *c = client.GetClient();
	if (c != nullptr)
		RegisterServices(c);
}

void
Publisher::OnAvahiConnect(AvahiClient *c) noexcept
{
	if (group == nullptr && visible)
		RegisterServices(c);
}

void
Publisher::OnAvahiDisconnect() noexcept
{
	group.reset();
}

void
Publisher::OnAvahiChanged() noexcept
{
	group.reset();
}

} // namespace Avahi
