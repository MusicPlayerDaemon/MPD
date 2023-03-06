// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "AllocatedSocketAddress.hxx"
#include "IPv4Address.hxx"
#include "IPv6Address.hxx"

#include <string.h>

#ifdef HAVE_UN
#include <sys/un.h>
#endif

AllocatedSocketAddress &
AllocatedSocketAddress::operator=(SocketAddress src) noexcept
{
	if (src.IsNull()) {
		Clear();
	} else {
		SetSize(src.GetSize());
		memcpy(address, src.GetAddress(), size);
	}

	return *this;
}

void
AllocatedSocketAddress::SetSize(size_type new_size) noexcept
{
	if (size == new_size)
		return;

	free(address);
	size = new_size;
	address = (struct sockaddr *)malloc(size);
}

#ifdef HAVE_UN

std::string_view
AllocatedSocketAddress::GetLocalRaw() const noexcept
{
	return SocketAddress(*this).GetLocalRaw();
}

void
AllocatedSocketAddress::SetLocal(const char *path) noexcept
{
	const bool is_abstract = *path == '@';

	/* sun_path must be null-terminated unless it's an abstract
	   socket */
	const size_t path_length = strlen(path) + !is_abstract;

	struct sockaddr_un *sun;
	SetSize(sizeof(*sun) - sizeof(sun->sun_path) + path_length);
	sun = (struct sockaddr_un *)address;
	sun->sun_family = AF_LOCAL;
	memcpy(sun->sun_path, path, path_length);

	if (is_abstract)
		sun->sun_path[0] = 0;
}

#endif

#ifdef HAVE_TCP

bool
AllocatedSocketAddress::SetPort(unsigned port) noexcept
{
	if (IsNull())
		return false;

	switch (GetFamily()) {
	case AF_INET:
		{
			auto &a = *(IPv4Address *)(void *)address;
			a.SetPort(port);
			return true;
		}

	case AF_INET6:
		{
			auto &a = *(IPv6Address *)(void *)address;
			a.SetPort(port);
			return true;
		}
	}

	return false;
}

#endif
