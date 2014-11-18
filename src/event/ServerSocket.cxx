/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "system/SocketUtil.hxx"
#include "system/SocketError.hxx"
#include "event/SocketMonitor.hxx"
#include "system/Resolver.hxx"
#include "system/fd_util.h"
#include "fs/AllocatedPath.hxx"
#include "fs/FileSystem.hxx"
#include "util/Alloc.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <string>
#include <algorithm>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#ifdef WIN32
#include <ws2tcpip.h>
#include <winsock.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#endif

#define DEFAULT_PORT	6600

class OneServerSocket final : private SocketMonitor {
	ServerSocket &parent;

	const unsigned serial;

	AllocatedPath path;

	size_t address_length;
	struct sockaddr *address;

public:
	OneServerSocket(EventLoop &_loop, ServerSocket &_parent,
			unsigned _serial,
			const struct sockaddr *_address,
			size_t _address_length)
		:SocketMonitor(_loop),
		 parent(_parent), serial(_serial),
		 path(AllocatedPath::Null()),
		 address_length(_address_length),
		 address((sockaddr *)xmemdup(_address, _address_length))
	{
		assert(_address != nullptr);
		assert(_address_length > 0);
	}

	OneServerSocket(const OneServerSocket &other) = delete;
	OneServerSocket &operator=(const OneServerSocket &other) = delete;

	~OneServerSocket() {
		free(address);

		if (IsDefined())
			Close();
	}

	unsigned GetSerial() const {
		return serial;
	}

	void SetPath(AllocatedPath &&_path) {
		assert(path.IsNull());

		path = std::move(_path);
	}

	bool Open(Error &error);

	using SocketMonitor::IsDefined;
	using SocketMonitor::Close;

	gcc_pure
	std::string ToString() const {
		return sockaddr_to_string(address, address_length);
	}

	void SetFD(int _fd) {
		SocketMonitor::Open(_fd);
		SocketMonitor::ScheduleRead();
	}

	void Accept();

private:
	virtual bool OnSocketReady(unsigned flags) override;
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
OneServerSocket::Accept()
{
	struct sockaddr_storage peer_address;
	size_t peer_address_length = sizeof(peer_address);
	int peer_fd =
		accept_cloexec_nonblock(Get(), (struct sockaddr*)&peer_address,
					&peer_address_length);
	if (peer_fd < 0) {
		const SocketErrorMessage msg;
		FormatError(server_socket_domain,
			    "accept() failed: %s", (const char *)msg);
		return;
	}

	if (socket_keepalive(peer_fd)) {
		const SocketErrorMessage msg;
		FormatError(server_socket_domain,
			    "Could not set TCP keepalive option: %s",
			    (const char *)msg);
	}

	parent.OnAccept(peer_fd,
			(const sockaddr &)peer_address,
			peer_address_length, get_remote_uid(peer_fd));
}

bool
OneServerSocket::OnSocketReady(gcc_unused unsigned flags)
{
	Accept();
	return true;
}

inline bool
OneServerSocket::Open(Error &error)
{
	assert(!IsDefined());

	int _fd = socket_bind_listen(address->sa_family,
				     SOCK_STREAM, 0,
				     address, address_length, 5,
				     error);
	if (_fd < 0)
		return false;

	/* allow everybody to connect */

	if (!path.IsNull())
		chmod(path.c_str(), 0666);

	/* register in the GLib main loop */

	SetFD(_fd);

	return true;
}

ServerSocket::ServerSocket(EventLoop &_loop)
	:loop(_loop), next_serial(1) {}

/* this is just here to allow the OneServerSocket forward
   declaration */
ServerSocket::~ServerSocket() {}

bool
ServerSocket::Open(Error &error)
{
	OneServerSocket *good = nullptr, *bad = nullptr;
	Error last_error;

	for (auto &i : sockets) {
		assert(i.GetSerial() > 0);
		assert(good == nullptr || i.GetSerial() >= good->GetSerial());

		if (bad != nullptr && i.GetSerial() != bad->GetSerial()) {
			Close();
			error = std::move(last_error);
			return false;
		}

		Error error2;
		if (!i.Open(error2)) {
			if (good != nullptr && good->GetSerial() == i.GetSerial()) {
				const auto address_string = i.ToString();
				const auto good_string = good->ToString();
				FormatWarning(server_socket_domain,
					      "bind to '%s' failed: %s "
					      "(continuing anyway, because "
					      "binding to '%s' succeeded)",
					      address_string.c_str(),
					      error2.GetMessage(),
					      good_string.c_str());
			} else if (bad == nullptr) {
				bad = &i;

				const auto address_string = i.ToString();
				error2.FormatPrefix("Failed to bind to '%s': ",
						    address_string.c_str());

				last_error = std::move(error2);
			}

			continue;
		}

		/* mark this socket as "good", and clear previous
		   errors */

		good = &i;

		if (bad != nullptr) {
			bad = nullptr;
			last_error.Clear();
		}
	}

	if (bad != nullptr) {
		Close();
		error = std::move(last_error);
		return false;
	}

	return true;
}

void
ServerSocket::Close()
{
	for (auto &i : sockets)
		if (i.IsDefined())
			i.Close();
}

OneServerSocket &
ServerSocket::AddAddress(const sockaddr &address, size_t address_length)
{
	sockets.emplace_back(loop, *this, next_serial,
			     &address, address_length);

	return sockets.back();
}

bool
ServerSocket::AddFD(int fd, Error &error)
{
	assert(fd >= 0);

	struct sockaddr_storage address;
	socklen_t address_length = sizeof(address);
	if (getsockname(fd, (struct sockaddr *)&address,
			&address_length) < 0) {
		SetSocketError(error);
		error.AddPrefix("Failed to get socket address: ");
		return false;
	}

	OneServerSocket &s = AddAddress((const sockaddr &)address,
					address_length);
	s.SetFD(fd);

	return true;
}

#ifdef HAVE_TCP

inline void
ServerSocket::AddPortIPv4(unsigned port)
{
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin_port = htons(port);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;

	AddAddress((const sockaddr &)sin, sizeof(sin));
}

#ifdef HAVE_IPV6

inline void
ServerSocket::AddPortIPv6(unsigned port)
{
	struct sockaddr_in6 sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin6_port = htons(port);
	sin.sin6_family = AF_INET6;

	AddAddress((const sockaddr &)sin, sizeof(sin));
}

/**
 * Is IPv6 supported by the kernel?
 */
gcc_pure
static bool
SupportsIPv6()
{
	int fd = socket(AF_INET6, SOCK_STREAM, 0);
	if (fd < 0)
		return false;

	close(fd);
	return true;
}

#endif /* HAVE_IPV6 */

#endif /* HAVE_TCP */

bool
ServerSocket::AddPort(unsigned port, Error &error)
{
#ifdef HAVE_TCP
	if (port == 0 || port > 0xffff) {
		error.Set(server_socket_domain, "Invalid TCP port");
		return false;
	}

#ifdef HAVE_IPV6
	if (SupportsIPv6())
		AddPortIPv6(port);
#endif
	AddPortIPv4(port);

	++next_serial;

	return true;
#else /* HAVE_TCP */
	(void)port;

	error.Set(server_socket_domain, "TCP support is disabled");
	return false;
#endif /* HAVE_TCP */
}

bool
ServerSocket::AddHost(const char *hostname, unsigned port, Error &error)
{
#ifdef HAVE_TCP
	struct addrinfo *ai = resolve_host_port(hostname, port,
						AI_PASSIVE, SOCK_STREAM,
						error);
	if (ai == nullptr)
		return false;

	for (const struct addrinfo *i = ai; i != nullptr; i = i->ai_next)
		AddAddress(*i->ai_addr, i->ai_addrlen);

	freeaddrinfo(ai);

	++next_serial;

	return true;
#else /* HAVE_TCP */
	(void)hostname;
	(void)port;

	error.Set(server_socket_domain, "TCP support is disabled");
	return false;
#endif /* HAVE_TCP */
}

bool
ServerSocket::AddPath(AllocatedPath &&path, Error &error)
{
#ifdef HAVE_UN
	struct sockaddr_un s_un;

	const size_t path_length = path.length();
	if (path_length >= sizeof(s_un.sun_path)) {
		error.Set(server_socket_domain,
			  "UNIX socket path is too long");
		return false;
	}

	RemoveFile(path);

	s_un.sun_family = AF_UNIX;
	memcpy(s_un.sun_path, path.c_str(), path_length + 1);

	OneServerSocket &s = AddAddress((const sockaddr &)s_un, sizeof(s_un));
	s.SetPath(std::move(path));

	return true;
#else /* !HAVE_UN */
	(void)path;

	error.Set(server_socket_domain,
		  "UNIX domain socket support is disabled");
	return false;
#endif /* !HAVE_UN */
}

