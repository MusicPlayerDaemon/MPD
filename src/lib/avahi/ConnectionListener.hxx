// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <avahi-client/client.h>

namespace Avahi {

class ConnectionListener {
public:
	/**
	 * The connection to the Avahi daemon has been established.
	 *
	 * Note that this may be called again after a collision
	 * (AVAHI_CLIENT_S_COLLISION) or a host name change
	 * (AVAHI_CLIENT_S_REGISTERING).
	 */
	virtual void OnAvahiConnect(AvahiClient *client) noexcept = 0;
	virtual void OnAvahiDisconnect() noexcept = 0;

	/**
	 * Something about the Avahi connection has changed, e.g. a
	 * collision (AVAHI_CLIENT_S_COLLISION) or a host name change
	 * (AVAHI_CLIENT_S_REGISTERING).  Services shall be
	 * unpublished now, and will be re-published in the following
	 * OnAvahiConnect() call.
	 */
	virtual void OnAvahiChanged() noexcept {}
};

} // namespace Avahi
