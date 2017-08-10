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

#ifndef SOCKET_DESCRIPTOR_HXX
#define SOCKET_DESCRIPTOR_HXX

#include "system/FileDescriptor.hxx"

#include <type_traits>

class SocketAddress;
class StaticSocketAddress;

/**
 * An OO wrapper for a UNIX socket descriptor.
 */
class SocketDescriptor : protected FileDescriptor {
protected:
	explicit constexpr SocketDescriptor(int _fd)
		:FileDescriptor(_fd) {}

	explicit constexpr SocketDescriptor(FileDescriptor _fd)
		:FileDescriptor(_fd) {}

public:
	SocketDescriptor() = default;

	constexpr bool operator==(SocketDescriptor other) const {
		return fd == other.fd;
	}

#ifndef _WIN32
	/**
	 * Convert a #FileDescriptor to a #SocketDescriptor.  This is only
	 * possible on operating systems where socket descriptors are the
	 * same as file descriptors (i.e. not on Windows).  Use this only
	 * when you know what you're doing.
	 */
	static constexpr SocketDescriptor FromFileDescriptor(FileDescriptor fd) {
		return SocketDescriptor(fd);
	}

	/**
	 * Convert this object to a #FileDescriptor instance.  This is only
	 * possible on operating systems where socket descriptors are the
	 * same as file descriptors (i.e. not on Windows).  Use this only
	 * when you know what you're doing.
	 */
	constexpr const FileDescriptor &ToFileDescriptor() const {
		return *this;
	}
#endif

	using FileDescriptor::IsDefined;
#ifndef _WIN32
	using FileDescriptor::IsValid;
#endif
	using FileDescriptor::Get;
	using FileDescriptor::Set;
	using FileDescriptor::Steal;
	using FileDescriptor::SetUndefined;

	static constexpr SocketDescriptor Undefined() {
		return SocketDescriptor(FileDescriptor::Undefined());
	}

#ifndef _WIN32
	using FileDescriptor::SetNonBlocking;
	using FileDescriptor::SetBlocking;
	using FileDescriptor::EnableCloseOnExec;
	using FileDescriptor::DisableCloseOnExec;
	using FileDescriptor::Duplicate;
	using FileDescriptor::Close;
#else
	/**
	 * This method replaces FileDescriptor::Close(), using closesocket()
	 * on Windows.  FileDescriptor::Close() is not virtual, so be
	 * careful when dealing with a FileDescriptor reference that is
	 * really a SocketDescriptor.
	 */
	void Close();
#endif

	/**
	 * Create a socket.
	 *
	 * @param domain is the address domain
	 * @param type is the sochet type
	 * @param protocol is the protocol
	 * @return True on success, False on failure
	 * See man 2 socket for detailed information
	 */
	bool Create(int domain, int type, int protocol);

	/**
	 * Like Create(), but enable non-blocking mode.
	 */
	bool CreateNonBlock(int domain, int type, int protocol);

	static bool CreateSocketPair(int domain, int type, int protocol,
				     SocketDescriptor &a, SocketDescriptor &b);
	static bool CreateSocketPairNonBlock(int domain, int type, int protocol,
					     SocketDescriptor &a, SocketDescriptor &b);

	int GetError();

	bool SetOption(int level, int name, const void *value, size_t size);

	bool SetBoolOption(int level, int name, bool _value) {
		const int value = _value;
		return SetOption(level, name, &value, sizeof(value));
	}

#ifdef __linux__
	bool SetReuseAddress(bool value=true);
	bool SetReusePort(bool value=true);
	bool SetFreeBind(bool value=true);
	bool SetNoDelay(bool value=true);
	bool SetCork(bool value=true);

	bool SetTcpDeferAccept(const int &seconds);
	bool SetV6Only(bool value);

	/**
	 * Setter for SO_BINDTODEVICE.
	 */
	bool SetBindToDevice(const char *name);

	bool SetTcpFastOpen(int qlen=16);
#endif

	bool Bind(SocketAddress address);

#ifdef __linux__
	/**
	 * Binds the socket to a unique abstract address.
	 */
	bool AutoBind();
#endif

	bool Listen(int backlog);

	SocketDescriptor Accept();
	SocketDescriptor AcceptNonBlock(StaticSocketAddress &address) const;

	bool Connect(SocketAddress address);

	gcc_pure
	StaticSocketAddress GetLocalAddress() const;

	gcc_pure
	StaticSocketAddress GetPeerAddress() const;

	ssize_t Read(void *buffer, size_t length);
	ssize_t Write(const void *buffer, size_t length);

#ifdef _WIN32
	int WaitReadable(int timeout_ms) const;
	int WaitWritable(int timeout_ms) const;
#else
	using FileDescriptor::WaitReadable;
	using FileDescriptor::WaitWritable;
	using FileDescriptor::IsReadyForWriting;
#endif

	/**
	 * Receive a datagram and return the source address.
	 */
	ssize_t Read(void *buffer, size_t length,
		     StaticSocketAddress &address);

	/**
	 * Send a datagram to the specified address.
	 */
	ssize_t Write(const void *buffer, size_t length,
		      SocketAddress address);
};

static_assert(std::is_trivial<SocketDescriptor>::value, "type is not trivial");

#endif
