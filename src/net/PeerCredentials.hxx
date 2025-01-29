// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "Features.hxx" // for HAVE_STRUCT_UCRED

#include <type_traits> // for std::is_trivial_v

#ifdef HAVE_STRUCT_UCRED
#include <sys/socket.h> // for struct ucred
#endif

/**
 * Portable wrapper for credentials of the process on the other side
 * of a (local) socket.
 */
class SocketPeerCredentials {
	friend class SocketDescriptor;

#ifdef HAVE_STRUCT_UCRED
	struct ucred cred;
#endif

public:
	constexpr SocketPeerCredentials() noexcept = default;

	static constexpr SocketPeerCredentials Undefined() noexcept {
		SocketPeerCredentials c;
#ifdef HAVE_STRUCT_UCRED
		c.cred.pid = 0;
		c.cred.uid = -1;
		c.cred.gid = -1;
#endif
		return c;
	}

	constexpr bool IsDefined() const noexcept {
#ifdef HAVE_STRUCT_UCRED
		return cred.pid > 0;
#else
		return false;
#endif
	}

	constexpr auto GetPid() const noexcept {
#ifdef HAVE_STRUCT_UCRED
		return cred.pid;
#else
		return 0;
#endif
	}

	constexpr auto GetUid() const noexcept {
#ifdef HAVE_STRUCT_UCRED
		return cred.uid;
#else
		return -1;
#endif
	}

	constexpr auto GetGid() const noexcept {
#ifdef HAVE_STRUCT_UCRED
		return cred.gid;
#else
		return -1;
#endif
	}
};

static_assert(std::is_trivial_v<SocketPeerCredentials>);
