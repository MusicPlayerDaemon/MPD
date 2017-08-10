/*
 * Copyright (C) 2012-2017 Max Kellermann <max.kellermann@gmail.com>
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

#include "config.h"
#include "SocketDescriptor.hxx"
#include "SocketAddress.hxx"
#include "StaticSocketAddress.hxx"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

#include <errno.h>
#include <string.h>

#ifdef _WIN32

void
SocketDescriptor::Close()
{
	if (IsDefined())
		::closesocket(Steal());
}

#endif

SocketDescriptor
SocketDescriptor::Accept()
{
#if defined(__linux__) && !defined(__BIONIC__) && !defined(KOBO)
	int connection_fd = ::accept4(Get(), nullptr, nullptr, SOCK_CLOEXEC);
#else
	int connection_fd = ::accept(Get(), nullptr, nullptr);
#endif
	return connection_fd >= 0
		? SocketDescriptor(connection_fd)
		: Undefined();
}

SocketDescriptor
SocketDescriptor::AcceptNonBlock(StaticSocketAddress &address) const
{
	address.SetMaxSize();
#if defined(__linux__) && !defined(__BIONIC__) && !defined(KOBO)
	int connection_fd = ::accept4(Get(), address, &address.size,
				      SOCK_CLOEXEC|SOCK_NONBLOCK);
#else
	int connection_fd = ::accept(Get(), address, &address.size);
#endif
	return SocketDescriptor(connection_fd);
}

bool
SocketDescriptor::Connect(SocketAddress address)
{
	assert(address.IsDefined());

	return ::connect(Get(), address.GetAddress(), address.GetSize()) >= 0;
}

bool
SocketDescriptor::Create(int domain, int type, int protocol)
{
#ifdef WIN32
	static bool initialised = false;
	if (!initialised) {
		WSADATA data;
		WSAStartup(MAKEWORD(2,2), &data);
		initialised = true;
	}
#endif

#ifdef SOCK_CLOEXEC
	/* implemented since Linux 2.6.27 */
	type |= SOCK_CLOEXEC;
#endif

	int new_fd = socket(domain, type, protocol);
	if (new_fd < 0)
		return false;

	Set(new_fd);
	return true;
}

bool
SocketDescriptor::CreateNonBlock(int domain, int type, int protocol)
{
#ifdef SOCK_NONBLOCK
	type |= SOCK_NONBLOCK;
#endif

	if (!Create(domain, type, protocol))
		return false;

#ifndef SOCK_NONBLOCK
	SetNonBlocking();
#endif

	return true;
}

bool
SocketDescriptor::CreateSocketPair(int domain, int type, int protocol,
				 SocketDescriptor &a, SocketDescriptor &b)
{
#ifdef SOCK_CLOEXEC
	/* implemented since Linux 2.6.27 */
	type |= SOCK_CLOEXEC;
#endif

	int fds[2];
	if (socketpair(domain, type, protocol, fds) < 0)
		return false;

	a = SocketDescriptor(fds[0]);
	b = SocketDescriptor(fds[1]);
	return true;
}

bool
SocketDescriptor::CreateSocketPairNonBlock(int domain, int type, int protocol,
					 SocketDescriptor &a, SocketDescriptor &b)
{
#ifdef SOCK_CLOEXEC
	/* implemented since Linux 2.6.27 */
	type |= SOCK_CLOEXEC;
#endif
	if (!CreateSocketPair(domain, type, protocol, a, b))
		return false;

#ifndef __linux__
	a.SetNonBlocking();
	b.SetNonBlocking();
#endif

	return true;
}

int
SocketDescriptor::GetError()
{
	assert(IsDefined());

	int s_err = 0;
	socklen_t s_err_size = sizeof(s_err);
	return getsockopt(fd, SOL_SOCKET, SO_ERROR,
			  (char *)&s_err, &s_err_size) == 0
		? s_err
		: errno;
}

#ifdef _WIN32

bool
SocketDescriptor::SetNonBlocking() noexcept
{
	u_long val = 1;
	return ioctlsocket(fd, FIONBIO, &val) == 0;
}

#endif

bool
SocketDescriptor::SetOption(int level, int name,
			    const void *value, size_t size)
{
	assert(IsDefined());

	return setsockopt(fd, level, name, value, size) == 0;
}

#ifdef __linux__

bool
SocketDescriptor::SetReuseAddress(bool value)
{
	return SetBoolOption(SOL_SOCKET, SO_REUSEADDR, value);
}

#ifdef SO_REUSEPORT

bool
SocketDescriptor::SetReusePort(bool value)
{
	return SetBoolOption(SOL_SOCKET, SO_REUSEPORT, value);
}

#endif

bool
SocketDescriptor::SetFreeBind(bool value)
{
	return SetBoolOption(IPPROTO_IP, IP_FREEBIND, value);
}

bool
SocketDescriptor::SetNoDelay(bool value)
{
	return SetBoolOption(IPPROTO_TCP, TCP_NODELAY, value);
}

bool
SocketDescriptor::SetCork(bool value)
{
	return SetBoolOption(IPPROTO_TCP, TCP_CORK, value);
}

bool
SocketDescriptor::SetTcpDeferAccept(const int &seconds)
{
	return SetOption(IPPROTO_TCP, TCP_DEFER_ACCEPT, &seconds, sizeof(seconds));
}

bool
SocketDescriptor::SetV6Only(bool value)
{
	return SetBoolOption(IPPROTO_IPV6, IPV6_V6ONLY, value);
}

bool
SocketDescriptor::SetBindToDevice(const char *name)
{
	return SetOption(SOL_SOCKET, SO_BINDTODEVICE, name, strlen(name));
}

#ifdef TCP_FASTOPEN

bool
SocketDescriptor::SetTcpFastOpen(int qlen)
{
	return SetOption(SOL_TCP, TCP_FASTOPEN, &qlen, sizeof(qlen));
}

#endif

#endif

bool
SocketDescriptor::Bind(SocketAddress address)
{
	return bind(Get(), address.GetAddress(), address.GetSize()) == 0;
}

#ifdef __linux__

bool
SocketDescriptor::AutoBind()
{
	static constexpr sa_family_t family = AF_LOCAL;
	return Bind(SocketAddress((const struct sockaddr *)&family,
				  sizeof(family)));
}

#endif

bool
SocketDescriptor::Listen(int backlog)
{
	return listen(Get(), backlog) == 0;
}

StaticSocketAddress
SocketDescriptor::GetLocalAddress() const
{
	assert(IsDefined());

	StaticSocketAddress result;
	result.size = result.GetCapacity();
	if (getsockname(fd, result, &result.size) < 0)
		result.Clear();

	return result;
}

StaticSocketAddress
SocketDescriptor::GetPeerAddress() const
{
	assert(IsDefined());

	StaticSocketAddress result;
	result.size = result.GetCapacity();
	if (getpeername(fd, result, &result.size) < 0)
		result.Clear();

	return result;
}

ssize_t
SocketDescriptor::Read(void *buffer, size_t length)
{
	int flags = 0;
#ifndef _WIN32
	flags |= MSG_DONTWAIT;
#endif

	return ::recv(Get(), (char *)buffer, length, flags);
}

ssize_t
SocketDescriptor::Write(const void *buffer, size_t length)
{
	int flags = 0;
#ifdef __linux__
	flags |= MSG_NOSIGNAL;
#endif

	return ::send(Get(), (const char *)buffer, length, flags);
}

#ifdef _WIN32

int
SocketDescriptor::WaitReadable(int timeout_ms) const
{
	assert(IsDefined());

	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(Get(), &rfds);

	struct timeval timeout, *timeout_p = nullptr;
	if (timeout_ms >= 0) {
		timeout.tv_sec = unsigned(timeout_ms) / 1000;
		timeout.tv_usec = (unsigned(timeout_ms) % 1000) * 1000;
		timeout_p = &timeout;
	}

	return select(Get() + 1, &rfds, nullptr, nullptr, timeout_p);
}

int
SocketDescriptor::WaitWritable(int timeout_ms) const
{
	assert(IsDefined());

	fd_set wfds;
	FD_ZERO(&wfds);
	FD_SET(Get(), &wfds);

	struct timeval timeout, *timeout_p = nullptr;
	if (timeout_ms >= 0) {
		timeout.tv_sec = unsigned(timeout_ms) / 1000;
		timeout.tv_usec = (unsigned(timeout_ms) % 1000) * 1000;
		timeout_p = &timeout;
	}

	return select(Get() + 1, nullptr, &wfds, nullptr, timeout_p);
}

#endif

ssize_t
SocketDescriptor::Read(void *buffer, size_t length,
		       StaticSocketAddress &address)
{
	int flags = 0;
#ifndef _WIN32
	flags |= MSG_DONTWAIT;
#endif

	socklen_t addrlen = address.GetCapacity();
	ssize_t nbytes = ::recvfrom(Get(), (char *)buffer, length, flags,
				    address, &addrlen);
	if (nbytes > 0)
		address.SetSize(addrlen);

	return nbytes;
}

ssize_t
SocketDescriptor::Write(const void *buffer, size_t length,
			SocketAddress address)
{
	int flags = 0;
#ifndef _WIN32
	flags |= MSG_DONTWAIT;
#endif
#ifdef __linux__
	flags |= MSG_NOSIGNAL;
#endif

	return ::sendto(Get(), (const char *)buffer, length, flags,
			address.GetAddress(), address.GetSize());
}
