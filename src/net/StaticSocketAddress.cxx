/*
 * Copyright (C) 2012-2015 Max Kellermann <max@duempel.org>
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

#include "StaticSocketAddress.hxx"

#include <algorithm>

#include <assert.h>
#include <string.h>

#ifdef WIN32
#include <ws2tcpip.h>
#else
#include <sys/un.h>
#include <sys/socket.h>
#endif

StaticSocketAddress &
StaticSocketAddress::operator=(SocketAddress other)
{
	size = std::min(other.GetSize(), GetCapacity());
	memcpy(&address, other.GetAddress(), size);
	return *this;
}

bool
StaticSocketAddress::operator==(const StaticSocketAddress &other) const
{
	return size == other.size &&
		memcmp(&address, &other.address, size) == 0;
}

#ifdef HAVE_UN

void
StaticSocketAddress::SetLocal(const char *path)
{
	auto &sun = reinterpret_cast<struct sockaddr_un &>(address);

	const size_t path_length = strlen(path);

	// TODO: make this a runtime check
	assert(path_length < sizeof(sun.sun_path));

	sun.sun_family = AF_LOCAL;
	memcpy(sun.sun_path, path, path_length + 1);

	/* note: Bionic doesn't provide SUN_LEN() */
	size = SUN_LEN(&sun);
}

#endif
