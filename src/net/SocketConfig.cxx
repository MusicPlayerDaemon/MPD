// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "SocketConfig.hxx"
#include "UniqueSocketDescriptor.hxx"
#include "SocketError.hxx"
#include "lib/fmt/SystemError.hxx"
#include "lib/fmt/SocketError.hxx"
#include "lib/fmt/SocketAddressFormatter.hxx"

#ifdef HAVE_TCP
#include "IPv4Address.hxx"
#endif

#include <cassert>

#ifdef HAVE_UN
#include <sys/stat.h>
#include <unistd.h>
#endif

void
SocketConfig::Fixup() noexcept
{
#if defined(HAVE_MULTICAST) && defined(HAVE_TCP) && defined(HAVE_IPV6)
	if (!bind_address.IsNull() && bind_address.IsV6Any() &&
	    !multicast_group.IsNull() &&
	    multicast_group.GetFamily() == AF_INET)
		bind_address = IPv4Address(bind_address.GetPort());
#endif // HAVE_MULTICAST
}

UniqueSocketDescriptor
SocketConfig::Create(int type) const
{
	assert(!bind_address.IsNull());
	assert(bind_address.IsDefined());

	const int family = bind_address.GetFamily();

#ifdef HAVE_TCP
	const bool is_tcp = bind_address.IsInet() && type == SOCK_STREAM;
#endif

#ifdef HAVE_MULTICAST
	const bool is_multicast = multicast_group.IsNull();
#elif defined(HAVE_TCP)
	constexpr bool is_multicast = false;
#endif


	int protocol = 0;

#if defined(__linux__) && defined(HAVE_TCP) && defined(IPPROTO_MPTCP)
	if (is_tcp && mptcp)
		protocol = IPPROTO_MPTCP;
#endif

	UniqueSocketDescriptor fd;
	if (!fd.CreateNonBlock(family, type, protocol))
		throw MakeSocketError("Failed to create socket");

#ifdef HAVE_UN
	const char *local_path = bind_address.GetLocalPath();
	if (local_path != nullptr)
		/* delete non-abstract socket files before reusing them */
		unlink(local_path);

	if (family == AF_LOCAL) {
		if (pass_cred)
			/* we want to receive the client's UID */
			fd.SetBoolOption(SOL_SOCKET, SO_PASSCRED, true);
	}
#endif

#ifdef HAVE_IPV6
	if (v6only)
		fd.SetV6Only(true);
	else if (bind_address.IsV6Any())
		fd.SetV6Only(false);
#endif // HAVE_IPV6

#ifdef HAVE_TCP
#ifdef __linux__
	if (!interface.empty() && !fd.SetBindToDevice(interface.c_str()))
		throw MakeSocketError("Failed to set SO_BINDTODEVICE");
#endif

	/* always set SO_REUSEADDR for TCP sockets to allow quick
	   restarts */
	/* set SO_REUSEADDR if we're using multicast; this option allows
	   multiple processes to join the same group on the same port */
	if ((is_tcp || !is_multicast) &&
	    !fd.SetReuseAddress(true))
		throw MakeSocketError("Failed to set SO_REUSEADDR");

#ifdef __linux__
	if (reuse_port && !fd.SetReusePort())
		throw MakeSocketError("Failed to set SO_REUSEPORT");

	if (free_bind && !fd.SetFreeBind())
		throw MakeSocketError("Failed to set SO_FREEBIND");
#endif
#endif // HAVE_TCP

#ifdef HAVE_UN
	if (mode != 0)
		/* use fchmod() on the unbound socket to limit the
		   mode, in order to avoid a race condition; later we
		   need to call chmod() on the socket path because the
		   bind() applies the umask */
		fchmod(fd.Get(), mode);
#endif

	if (!fd.Bind(bind_address))
		throw FmtSocketError("Failed to bind to {}", bind_address);

#ifdef HAVE_UN
	if (mode != 0 && local_path != nullptr && chmod(local_path, mode) < 0)
		throw FmtErrno("Failed to chmod {:?}", local_path);
#endif

#ifdef HAVE_MULTICAST
	if (!multicast_group.IsNull() &&
	    !fd.AddMembership(multicast_group))
		throw FmtSocketError("Failed to join multicast group {}",
				     multicast_group);
#endif

#ifdef HAVE_TCP
	if (is_tcp) {
#ifdef __linux__
		fd.SetTcpFastOpen();

		if (tcp_defer_accept > 0)
			fd.SetTcpDeferAccept(tcp_defer_accept);

		if (tcp_user_timeout > 0)
			fd.SetTcpUserTimeout(tcp_user_timeout);
#endif

		if (tcp_no_delay)
			fd.SetNoDelay();
	}

	if (keepalive)
		fd.SetKeepAlive();
#endif // HAVE_TCP

	if (listen > 0 && !fd.Listen(listen))
		throw MakeSocketError("Failed to listen");

	return fd;
}
