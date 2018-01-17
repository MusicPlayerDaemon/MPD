/*
 * Copyright 2003-2017 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "ServerSocket.hxx"
#include "net/IPv4Address.hxx"
#include "net/StaticSocketAddress.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "net/SocketAddress.hxx"
#include "net/SocketUtil.hxx"
#include "net/SocketError.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/Resolver.hxx"
#include "net/ToString.hxx"
#include "event/SocketMonitor.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/FileSystem.hxx"
#include "util/RuntimeError.hxx"
#include "util/Domain.hxx"
#include "util/ScopeExit.hxx"
#include "Log.hxx"

#include <string>
#include <algorithm>

#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#ifdef _WIN32
#include <ws2tcpip.h>
#include <winsock.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#endif

class OneServerSocket final : private SocketMonitor {
	ServerSocket &parent;

	const unsigned serial;

#ifdef HAVE_UN
	AllocatedPath path;
#endif

	const AllocatedSocketAddress address;

public:
	template<typename A>
	OneServerSocket(EventLoop &_loop, ServerSocket &_parent,
			unsigned _serial,
			A &&_address)
		:SocketMonitor(_loop),
		 parent(_parent), serial(_serial),
#ifdef HAVE_UN
		 path(nullptr),
#endif
		 address(std::forward<A>(_address))
	{
	}

	OneServerSocket(const OneServerSocket &other) = delete;
	OneServerSocket &operator=(const OneServerSocket &other) = delete;

	~OneServerSocket() {
		if (IsDefined())
			Close();
	}

	unsigned GetSerial() const {
		return serial;
	}

#ifdef HAVE_UN
	void SetPath(AllocatedPath &&_path) {
		assert(path.IsNull());

		path = std::move(_path);
	}
#endif

	void Open();

	using SocketMonitor::IsDefined;
	using SocketMonitor::Close;

	gcc_pure
	std::string ToString() const noexcept {
		return ::ToString(address);
	}

	void SetFD(SocketDescriptor _fd) noexcept {
		SocketMonitor::Open(_fd);
		SocketMonitor::ScheduleRead();
	}

	void Accept() noexcept;

private:
	bool OnSocketReady(unsigned flags) noexcept override;
};

static constexpr Domain server_socket_domain("server_socket");

static int
get_remote_uid(int fd)
{
#ifdef HAVE_STRUCT_UCRED
	struct ucred cred;
	socklen_t len = sizeof (cred);

	if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) < 0)
		return -1;

	return cred.uid;
#else
#ifdef HAVE_GETPEEREID
	uid_t euid;
	gid_t egid;

	if (getpeereid(fd, &euid, &egid) == 0)
		return euid;
#else
	(void)fd;
#endif
	return -1;
#endif
}

inline void
OneServerSocket::Accept() noexcept
{
	StaticSocketAddress peer_address;
	UniqueSocketDescriptor peer_fd(GetSocket().AcceptNonBlock(peer_address));
	if (!peer_fd.IsDefined()) {
		const SocketErrorMessage msg;
		FormatError(server_socket_domain,
			    "accept() failed: %s", (const char *)msg);
		return;
	}

	if (!peer_fd.SetKeepAlive()) {
		const SocketErrorMessage msg;
		FormatError(server_socket_domain,
			    "Could not set TCP keepalive option: %s",
			    (const char *)msg);
	}

	parent.OnAccept(std::move(peer_fd), peer_address,
			get_remote_uid(peer_fd.Get()));
}

bool
OneServerSocket::OnSocketReady(gcc_unused unsigned flags) noexcept
{
	Accept();
	return true;
}

inline void
OneServerSocket::Open()
{
	assert(!IsDefined());

	auto _fd = socket_bind_listen(address.GetFamily(),
				      SOCK_STREAM, 0,
				      address, 5);

#ifdef HAVE_UN
	/* allow everybody to connect */

	if (!path.IsNull())
		chmod(path.c_str(), 0666);
#endif

	/* register in the EventLoop */

	SetFD(_fd.Release());
}

ServerSocket::ServerSocket(EventLoop &_loop)
	:loop(_loop), next_serial(1) {}

/* this is just here to allow the OneServerSocket forward
   declaration */
ServerSocket::~ServerSocket() {}

void
ServerSocket::Open()
{
	OneServerSocket *good = nullptr, *bad = nullptr;
	std::exception_ptr last_error;

	for (auto &i : sockets) {
		assert(i.GetSerial() > 0);
		assert(good == nullptr || i.GetSerial() >= good->GetSerial());

		if (bad != nullptr && i.GetSerial() != bad->GetSerial()) {
			Close();
			std::rethrow_exception(last_error);
		}

		try {
			i.Open();
		} catch (...) {
			if (good != nullptr && good->GetSerial() == i.GetSerial()) {
				const auto address_string = i.ToString();
				const auto good_string = good->ToString();
				FormatError(std::current_exception(),
					    "bind to '%s' failed "
					    "(continuing anyway, because "
					    "binding to '%s' succeeded)",
					    address_string.c_str(),
					    good_string.c_str());
			} else if (bad == nullptr) {
				bad = &i;

				const auto address_string = i.ToString();

				try {
					std::throw_with_nested(FormatRuntimeError("Failed to bind to '%s'",
										  address_string.c_str()));
				} catch (...) {
					last_error = std::current_exception();
				}
			}

			continue;
		}

		/* mark this socket as "good", and clear previous
		   errors */

		good = &i;

		if (bad != nullptr) {
			bad = nullptr;
			last_error = nullptr;
		}
	}

	if (bad != nullptr) {
		Close();
		std::rethrow_exception(last_error);
	}
}

void
ServerSocket::Close()
{
	for (auto &i : sockets)
		if (i.IsDefined())
			i.Close();
}

OneServerSocket &
ServerSocket::AddAddress(SocketAddress address)
{
	sockets.emplace_back(loop, *this, next_serial,
			     address);

	return sockets.back();
}

OneServerSocket &
ServerSocket::AddAddress(AllocatedSocketAddress &&address)
{
	sockets.emplace_back(loop, *this, next_serial,
			     std::move(address));

	return sockets.back();
}

void
ServerSocket::AddFD(int _fd)
{
	assert(_fd >= 0);

	SocketDescriptor fd(_fd);

	StaticSocketAddress address = fd.GetLocalAddress();
	if (!address.IsDefined())
		throw MakeSocketError("Failed to get socket address");

	OneServerSocket &s = AddAddress(address);
	s.SetFD(fd);
}

#ifdef HAVE_TCP

inline void
ServerSocket::AddPortIPv4(unsigned port)
{
	AddAddress(IPv4Address(port));
}

#ifdef HAVE_IPV6

inline void
ServerSocket::AddPortIPv6(unsigned port)
{
	struct sockaddr_in6 sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin6_port = htons(port);
	sin.sin6_family = AF_INET6;

	AddAddress({(const sockaddr *)&sin, sizeof(sin)});
}

/**
 * Is IPv6 supported by the kernel?
 */
gcc_pure
static bool
SupportsIPv6() noexcept
{
	int fd = socket(AF_INET6, SOCK_STREAM, 0);
	if (fd < 0)
		return false;

	close(fd);
	return true;
}

#endif /* HAVE_IPV6 */

#endif /* HAVE_TCP */

void
ServerSocket::AddPort(unsigned port)
{
#ifdef HAVE_TCP
	if (port == 0 || port > 0xffff)
		throw std::runtime_error("Invalid TCP port");

#ifdef HAVE_IPV6
	if (SupportsIPv6())
		AddPortIPv6(port);
#endif
	AddPortIPv4(port);

	++next_serial;
#else /* HAVE_TCP */
	(void)port;

	throw std::runtime_error("TCP support is disabled");
#endif /* HAVE_TCP */
}

void
ServerSocket::AddHost(const char *hostname, unsigned port)
{
#ifdef HAVE_TCP
	struct addrinfo *ai = resolve_host_port(hostname, port,
						AI_PASSIVE, SOCK_STREAM);
	AtScopeExit(ai) { freeaddrinfo(ai); };

	for (const struct addrinfo *i = ai; i != nullptr; i = i->ai_next)
		AddAddress(SocketAddress(i->ai_addr, i->ai_addrlen));

	++next_serial;
#else /* HAVE_TCP */
	(void)hostname;
	(void)port;

	throw std::runtime_error("TCP support is disabled");
#endif /* HAVE_TCP */
}

void
ServerSocket::AddPath(AllocatedPath &&path)
{
#ifdef HAVE_UN
	unlink(path.c_str());

	AllocatedSocketAddress address;
	address.SetLocal(path.c_str());

	OneServerSocket &s = AddAddress(std::move(address));
	s.SetPath(std::move(path));
#else /* !HAVE_UN */
	(void)path;

	throw std::runtime_error("UNIX domain socket support is disabled");
#endif /* !HAVE_UN */
}

