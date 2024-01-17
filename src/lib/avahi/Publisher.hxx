// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "EntryGroup.hxx"
#include "ConnectionListener.hxx"
#include "event/DeferEvent.hxx"

#include <avahi-client/publish.h>

#include <string>
#include <forward_list>

class SocketAddress;

namespace Avahi {

struct Service;
class ErrorHandler;
class Client;

/**
 * A helper class which manages a list of services to be published via
 * Avahi/Zeroconf.
 */
class Publisher final : ConnectionListener {
	ErrorHandler &error_handler;

	std::string name;

	Client &client;

	DeferEvent defer_register_services;

	EntryGroupPtr group;

	const std::forward_list<Service> services;

	/**
	 * Shall the published services be visible?  This is controlled by
	 * HideServices() and ShowServices().
	 */
	bool visible = true;

public:
	Publisher(Client &client, const char *_name,
		  std::forward_list<Service> _services,
		  ErrorHandler &_error_handler) noexcept;
	~Publisher() noexcept;

	Publisher(const Publisher &) = delete;
	Publisher &operator=(const Publisher &) = delete;

	/**
	 * Temporarily hide all registered services.  You can undo this
	 * with ShowServices().
	 */
	void HideServices() noexcept;

	/**
	 * Undo HideServices().
	 */
	void ShowServices() noexcept;

private:
	void GroupCallback(AvahiEntryGroup *g,
			   AvahiEntryGroupState state) noexcept;
	static void GroupCallback(AvahiEntryGroup *g,
				  AvahiEntryGroupState state,
				  void *userdata) noexcept;

	void RegisterServices(AvahiEntryGroup &g);
	void RegisterServices(AvahiClient *c);
	void DeferredRegisterServices() noexcept;

	/* virtual methods from class AvahiConnectionListener */
	void OnAvahiConnect(AvahiClient *client) noexcept override;
	void OnAvahiDisconnect() noexcept override;
	void OnAvahiChanged() noexcept override;
};

} // namespace Avahi
