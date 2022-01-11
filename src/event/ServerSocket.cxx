/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "ServerSocket.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "net/IPv4Address.hxx"
#include "net/IPv6Address.hxx"
#include "net/StaticSocketAddress.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "net/SocketUtil.hxx"
#include "net/SocketError.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/Resolver.hxx"
#include "net/AddressInfo.hxx"
#include "net/ToString.hxx"
#include "event/SocketEvent.hxx"
#include "fs/AllocatedPath.hxx"
#include "util/RuntimeError.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <string>
#include <utility>

#ifdef HAVE_UN
#include <sys/stat.h>
#endif

class ServerSocket::OneServerSocket final {
	ServerSocket &parent;

	SocketEvent event;

	const unsigned serial;

#ifdef HAVE_UN
	AllocatedPath path;
#endif

	const AllocatedSocketAddress address;

public:
	template<typename A>
	OneServerSocket(EventLoop &_loop, ServerSocket &_parent,
			unsigned _serial,
			A &&_address) noexcept
		:parent(_parent),
		 event(_loop, BIND_THIS_METHOD(OnSocketReady)),
		 serial(_serial),
#ifdef HAVE_UN
		 path(nullptr),
#endif
		 address(std::forward<A>(_address))
	{
	}

	OneServerSocket(const OneServerSocket &other) = delete;
	OneServerSocket &operator=(const OneServerSocket &other) = delete;

	~OneServerSocket() noexcept {
		Close();
	}

	[[nodiscard]] unsigned GetSerial() const noexcept {
		return serial;
	}

#ifdef HAVE_UN
	void SetPath(AllocatedPath &&_path) noexcept {
		assert(path.IsNull());

		path = std::move(_path);
	}
#endif

	[[nodiscard]] bool IsDefined() const noexcept {
		return event.IsDefined();
	}

	void Open();

	void Close() noexcept {
		event.Close();
	}

	[[nodiscard]] [[gnu::pure]]
	std::string ToString() const noexcept {
		return ::ToString(address);
	}

	void SetFD(UniqueSocketDescriptor _fd) noexcept {
		event.Open(_fd.Release());
		event.ScheduleRead();
	}

	void Accept() noexcept;

private:
	void OnSocketReady(unsigned flags) noexcept;
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
ServerSocket::OneServerSocket::Accept() noexcept
{
	StaticSocketAddress peer_address;
	UniqueSocketDescriptor peer_fd(event.GetSocket().AcceptNonBlock(peer_address));
	if (!peer_fd.IsDefined()) {
		const SocketErrorMessage msg;
		FmtError(server_socket_domain,
			 "accept() failed: {}", (const char *)msg);
		return;
	}

	if (!peer_fd.SetKeepAlive()) {
		const SocketErrorMessage msg;
		FmtError(server_socket_domain,
			 "Could not set TCP keepalive option: {}",
			 (const char *)msg);
	}

	const auto uid = get_remote_uid(peer_fd.Get());

	parent.OnAccept(std::move(peer_fd), peer_address, uid);
}

void
ServerSocket::OneServerSocket::OnSocketReady([[maybe_unused]] unsigned flags) noexcept
{
	Accept();
}

inline void
ServerSocket::OneServerSocket::Open()
{
	assert(!IsDefined());

	auto _fd = socket_bind_listen(address.GetFamily(),
				      SOCK_STREAM, 0,
				      address, 5);

#ifdef HAVE_TCP
	if (parent.dscp_class >= 0) {
		const int family = address.GetFamily();
		if ((family == AF_INET &&
		     !_fd.SetIntOption(IPPROTO_IP, IP_TOS, parent.dscp_class)) ||
		    (family == AF_INET6 &&
		     !_fd.SetIntOption(IPPROTO_IPV6, IPV6_TCLASS,
				       parent.dscp_class))) {
			const SocketErrorMessage msg;
			FmtError(server_socket_domain,
				 "Could not set DSCP class: {}",
				 (const char *)msg);
		}
	}
#endif

#ifdef HAVE_UN
	/* allow everybody to connect */

	if (!path.IsNull())
		chmod(path.c_str(), 0666);
#endif

	/* register in the EventLoop */	

	SetFD(std::move(_fd));
}

ServerSocket::ServerSocket(EventLoop &_loop) noexcept
	:loop(_loop) {}

/* this is just here to allow the OneServerSocket forward
   declaration */
ServerSocket::~ServerSocket() noexcept = default;

void
ServerSocket::Open()
{
	OneServerSocket *good = nullptr, *bad = nullptr;
	std::exception_ptr last_error;

	for (auto &i : sockets) {
		assert(i.GetSerial() > 0);
		assert(good == nullptr || i.GetSerial() >= good->GetSerial());

		if (i.IsDefined())
			/* already open - was probably added by
			   AddFD() */
			continue;

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
				FmtError(server_socket_domain,
					 "bind to '{}' failed "
					 "(continuing anyway, because "
					 "binding to '{}' succeeded): {}",
					 address_string,
					 good_string,
					 std::current_exception());
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
ServerSocket::Close() noexcept
{
	for (auto &i : sockets)
		if (i.IsDefined())
			i.Close();
}

template<typename A>
ServerSocket::OneServerSocket &
ServerSocket::AddAddress(A &&address) noexcept
{
	sockets.emplace_back(loop, *this, next_serial,
			     std::forward<A>(address));

	return sockets.back();
}

void
ServerSocket::AddFD(UniqueSocketDescriptor fd)
{
	assert(fd.IsDefined());

	StaticSocketAddress address = fd.GetLocalAddress();
	if (!address.IsDefined())
		throw MakeSocketError("Failed to get socket address");

	OneServerSocket &s = AddAddress(address);
	s.SetFD(std::move(fd));
}

void
ServerSocket::AddFD(UniqueSocketDescriptor fd,
		    AllocatedSocketAddress &&address) noexcept
{
	assert(fd.IsDefined());
	assert(!address.IsNull());
	assert(address.IsDefined());

	OneServerSocket &s = AddAddress(std::move(address));
	s.SetFD(std::move(fd));
}

#ifdef HAVE_TCP

inline void
ServerSocket::AddPortIPv4(unsigned port) noexcept
{
	AddAddress(IPv4Address(port));
}

#ifdef HAVE_IPV6

inline void
ServerSocket::AddPortIPv6(unsigned port) noexcept
{
	AddAddress(IPv6Address(port));
}

/**
 * Is IPv6 supported by the kernel?
 */
[[gnu::pure]]
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
	for (const auto &i : Resolve(hostname, port,
				     AI_PASSIVE, SOCK_STREAM))
		AddAddress(i);

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

	throw std::runtime_error("Local socket support is disabled");
#endif /* !HAVE_UN */
}


void
ServerSocket::AddAbstract(const char *name)
{
#if !defined(__linux__)
	(void)name;

	throw std::runtime_error("Abstract sockets are only available on Linux");
#elif !defined(HAVE_UN)
	(void)name;

	throw std::runtime_error("Local socket support is disabled");
#else
	assert(name != nullptr);
	assert(*name == '@');

	AllocatedSocketAddress address;
	address.SetLocal(name);

	AddAddress(std::move(address));
#endif
}
