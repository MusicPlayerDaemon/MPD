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
