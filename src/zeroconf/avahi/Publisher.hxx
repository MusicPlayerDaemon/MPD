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

#pragma once

#include "EntryGroup.hxx"
#include "ConnectionListener.hxx"

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

	void RegisterServices(AvahiClient *c) noexcept;

	/* virtual methods from class AvahiConnectionListener */
	void OnAvahiConnect(AvahiClient *client) noexcept override;
	void OnAvahiDisconnect() noexcept override;
	void OnAvahiChanged() noexcept override;
};

} // namespace Avahi
