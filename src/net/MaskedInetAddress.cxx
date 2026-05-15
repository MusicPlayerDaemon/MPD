// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "MaskedInetAddress.hxx"
#include "SocketAddress.hxx"

#include <stdlib.h> // for strtoul()
#include <string.h> // for strchr(), memcpy()

bool
MaskedInetAddress::Matches(SocketAddress _other) const noexcept
{
	BareInetAddress other;
	return other.CopyFrom(_other) && Matches(other);
}

bool
MaskedInetAddress::Parse(const char *s) noexcept
{
	const char *slash = strchr(s, '/');
	if (slash == nullptr) {
		prefix_length = 128;
		return address.Parse(s);
	}

	/* copy the address part to a new buffer so we can
	   null-terminate it for BareInetAddress::Parse() */
	std::size_t address_length = slash - s;
	char buffer[48];
	if (address_length >= sizeof(buffer))
		return false;

	memcpy(buffer, s, address_length);
	buffer[address_length] = 0;

	if (!address.Parse(buffer))
		return false;

	const char *p = slash + 1;
	char *endptr;

	unsigned long pl = strtoul(p, &endptr, 10);
	if (address.IsV4Mapped())
		pl += 96;

	if (endptr == p || *endptr != 0 || pl > 128)
		return false;

	prefix_length = pl;
	return address == address.ToNetwork(prefix_length);
}

const char *
MaskedInetAddress::Format(std::span<char> buffer) const noexcept
{
	const char *s = address.Format(buffer);
	if (s == nullptr)
		return nullptr;

	if (prefix_length < 128) {
		assert(s == buffer.data());
		assert(strlen(s) < buffer.size());
		buffer = buffer.subspan(strlen(s));

		unsigned pl = prefix_length;
		if (address.IsV4Mapped()) {
			if (pl < 96)
				return nullptr;

			pl -= 96;
		}

		int l = snprintf(buffer.data(), buffer.size(), "/%u", pl);
		if (l >= static_cast<int>(buffer.size()))
			/* truncated */
			return nullptr;
	}

	return s;
}
