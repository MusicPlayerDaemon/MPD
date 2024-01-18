// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "util/IntrusiveList.hxx"

#include <avahi-common/address.h>

#include <cstdint>
#include <string>

namespace Avahi {

/**
 * A service that will be published by class #Publisher.
 */
struct Service : IntrusiveListHook<> {
	AvahiIfIndex interface = AVAHI_IF_UNSPEC;
	AvahiProtocol protocol = AVAHI_PROTO_UNSPEC;
	std::string type;
	uint16_t port;

	/**
	 * If this is false, then the service is not published.  You
	 * can change this field at any time and then call
	 * Publisher::UpdateServices() to publish the change.
	 */
	bool visible = true;

	Service(AvahiIfIndex _interface, AvahiProtocol _protocol,
		const char *_type, uint16_t _port) noexcept
		:interface(_interface), protocol(_protocol),
		 type(_type), port(_port) {}
};

} // namespace Avahi
