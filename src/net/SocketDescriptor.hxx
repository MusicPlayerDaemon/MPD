// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef SOCKET_DESCRIPTOR_HXX
#define SOCKET_DESCRIPTOR_HXX

#include "Features.hxx"

#ifndef _WIN32
#include "io/FileDescriptor.hxx"
#endif

#include <cstddef>
#include <span>
#include <type_traits>
#include <utility>

#ifdef _WIN32
#include <winsock2.h> // for SOCKET, INVALID_SOCKET
#endif

class SocketAddress;
class StaticSocketAddress;
class IPv4Address;
class IPv6Address;

/**
 * An OO wrapper for a Berkeley or WinSock socket descriptor.
 */
class SocketDescriptor
#ifndef _WIN32
/* Berkeley sockets are represented as file descriptors */
	: protected FileDescriptor
#endif
{
protected:
#ifdef _WIN32
	/* WinSock sockets are not file descriptors, they are a
	   special type */
	SOCKET fd;
#else // !_WIN32
	explicit constexpr SocketDescriptor(FileDescriptor _fd) noexcept
		:FileDescriptor(_fd) {}
#endif // !_WIN32

public:
	SocketDescriptor() = default;

#ifdef _WIN32
	explicit constexpr SocketDescriptor(SOCKET _fd) noexcept
		:fd(_fd) {}
#else // !_WIN32
	explicit constexpr SocketDescriptor(int _fd) noexcept
		:FileDescriptor(_fd) {}
#endif // !_WIN32

	constexpr bool operator==(SocketDescriptor other) const noexcept {
		return fd == other.fd;
	}

#ifdef _WIN32
	constexpr bool IsDefined() const noexcept {
		return fd != INVALID_SOCKET;
	}
#else // !_WIN32
	/**
	 * Convert a #FileDescriptor to a #SocketDescriptor.  This is only
	 * possible on operating systems where socket descriptors are the
	 * same as file descriptors (i.e. not on Windows).  Use this only
	 * when you know what you're doing.
	 */
	static constexpr SocketDescriptor FromFileDescriptor(FileDescriptor fd) noexcept {
		return SocketDescriptor(fd);
	}

	/**
	 * Convert this object to a #FileDescriptor instance.  This is only
	 * possible on operating systems where socket descriptors are the
	 * same as file descriptors (i.e. not on Windows).  Use this only
	 * when you know what you're doing.
	 */
	constexpr const FileDescriptor &ToFileDescriptor() const noexcept {
		return *this;
	}

	using FileDescriptor::IsDefined;
	using FileDescriptor::IsValid;
	using FileDescriptor::IsSocket;
#endif // !_WIN32

	/**
	 * Determine the socket type, i.e. SOCK_STREAM, SOCK_DGRAM or
	 * SOCK_SEQPACKET.  Returns -1 on error.
	 */
	[[gnu::pure]]
	int GetType() const noexcept;

	/**
	 * Is this a stream socket?
	 */
	[[gnu::pure]]
	bool IsStream() const noexcept;

#ifndef _WIN32
	/**
	 * Determine the socket protocol (SO_PROTOCOL),
	 * e.g. IPPROTO_SCTP.  Returns -1 on error.
	 */
	[[gnu::pure]]
	int GetProtocol() const noexcept;
#endif // !_WIN32

	static constexpr SocketDescriptor Undefined() noexcept {
#ifdef _WIN32
		return SocketDescriptor{INVALID_SOCKET};
#else // !_WIN32
		return SocketDescriptor(FileDescriptor::Undefined());
#endif // !_WIN32
	}

#ifndef _WIN32
	using FileDescriptor::Get;
	using FileDescriptor::Set;
	using FileDescriptor::Steal;
	using FileDescriptor::SetUndefined;

	using FileDescriptor::EnableCloseOnExec;
	using FileDescriptor::DisableCloseOnExec;

	using FileDescriptor::SetNonBlocking;
	using FileDescriptor::SetBlocking;
	using FileDescriptor::Duplicate;
	using FileDescriptor::CheckDuplicate;
	using FileDescriptor::Close;
#else
	constexpr SOCKET Get() const noexcept {
		return fd;
	}

	constexpr void Set(SOCKET _fd) noexcept {
		fd = _fd;
	}

	constexpr void SetUndefined() noexcept {
		fd = INVALID_SOCKET;
	}

	constexpr SOCKET Steal() noexcept {
		return std::exchange(fd, INVALID_SOCKET);
	}

	void EnableCloseOnExec() const noexcept {}
	void DisableCloseOnExec() const noexcept {}

	bool SetNonBlocking() const noexcept;

	/**
	 * This method replaces FileDescriptor::Close(), using closesocket()
	 * on Windows.  FileDescriptor::Close() is not virtual, so be
	 * careful when dealing with a FileDescriptor reference that is
	 * really a SocketDescriptor.
	 */
	void Close() noexcept;
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
	bool Create(int domain, int type, int protocol) noexcept;

	/**
	 * Like Create(), but enable non-blocking mode.
	 */
	bool CreateNonBlock(int domain, int type, int protocol) noexcept;

#ifndef _WIN32
	static bool CreateSocketPair(int domain, int type, int protocol,
				     SocketDescriptor &a,
				     SocketDescriptor &b) noexcept;
	static bool CreateSocketPairNonBlock(int domain, int type, int protocol,
					     SocketDescriptor &a,
					     SocketDescriptor &b) noexcept;
#endif

	[[gnu::pure]]
	int GetError() const noexcept;

	/**
	 * @return the value size or 0 on error
	 */
	std::size_t GetOption(int level, int name,
			      void *value, std::size_t size) const noexcept;

	[[gnu::pure]]
	int GetIntOption(int level, int name, int fallback) const noexcept;

#ifdef HAVE_STRUCT_UCRED
	/**
	 * Receive peer credentials (SO_PEERCRED).  On error, the pid
	 * is -1.
	 */
	[[gnu::pure]]
	struct ucred GetPeerCredentials() const noexcept;
#endif

	bool SetOption(int level, int name,
		       const void *value, std::size_t size) const noexcept;

	bool SetIntOption(int level, int name,
			  const int &value) const noexcept {
		return SetOption(level, name, &value, sizeof(value));
	}

	bool SetBoolOption(int level, int name, bool value) const noexcept {
		return SetIntOption(level, name, value);
	}

	bool SetKeepAlive(bool value=true) const noexcept;
	bool SetReuseAddress(bool value=true) const noexcept;

#ifdef __linux__
	bool SetReusePort(bool value=true) const noexcept;
	bool SetFreeBind(bool value=true) const noexcept;
	bool SetNoDelay(bool value=true) const noexcept;
	bool SetCork(bool value=true) const noexcept;

	bool SetTcpDeferAccept(const int &seconds) const noexcept;

	/**
	 * Setter for TCP_USER_TIMEOUT.
	 */
	bool SetTcpUserTimeout(const unsigned &milliseconds) const noexcept;

	bool SetV6Only(bool value) const noexcept;

	/**
	 * Setter for SO_BINDTODEVICE.
	 */
	bool SetBindToDevice(const char *name) const noexcept;

	bool SetTcpFastOpen(int qlen=16) const noexcept;

	bool AddMembership(const IPv4Address &address) const noexcept;
	bool AddMembership(const IPv6Address &address) const noexcept;
	bool AddMembership(SocketAddress address) const noexcept;
#endif

	bool Bind(SocketAddress address) const noexcept;

#ifdef __linux__
	/**
	 * Binds the socket to a unique abstract address.
	 */
	bool AutoBind() const noexcept;
#endif

	bool Listen(int backlog) const noexcept;

	SocketDescriptor Accept() const noexcept;
	SocketDescriptor AcceptNonBlock() const noexcept;
	SocketDescriptor AcceptNonBlock(StaticSocketAddress &address) const noexcept;

	bool Connect(SocketAddress address) const noexcept;

	[[gnu::pure]]
	StaticSocketAddress GetLocalAddress() const noexcept;

	[[gnu::pure]]
	StaticSocketAddress GetPeerAddress() const noexcept;

	/**
	 * Wrapper for recv().
	 */
	ssize_t Receive(std::span<std::byte> dest, int flags=0) const noexcept;

	/**
	 * Wrapper for send().
	 *
	 * MSG_NOSIGNAL is implicitly added (if available).
	 */
	ssize_t Send(std::span<const std::byte> src, int flags=0) const noexcept;

	ssize_t Read(std::span<std::byte> dest) const noexcept {
		return Receive(dest);
	}

	ssize_t Write(std::span<const std::byte> src) const noexcept {
		return Send(src);
	}

	/**
	 * Wrapper for Receive() with MSG_DONTWAIT (not available on
	 * Windows).
	 */
	ssize_t ReadNoWait(std::span<std::byte> dest) const noexcept;

	/**
	 * Wrapper for Receive() with MSG_DONTWAIT (not available on
	 * Windows).
	 */
	ssize_t WriteNoWait(std::span<const std::byte> src) const noexcept;

#ifdef _WIN32
	int WaitReadable(int timeout_ms) const noexcept;
	int WaitWritable(int timeout_ms) const noexcept;
#else
	using FileDescriptor::WaitReadable;
	using FileDescriptor::WaitWritable;
	using FileDescriptor::IsReadyForWriting;
#endif

	/**
	 * Receive a datagram and return the source address.
	 */
	ssize_t Read(void *buffer, std::size_t length,
		     StaticSocketAddress &address) const noexcept;

	/**
	 * Send a datagram to the specified address.
	 */
	ssize_t Write(const void *buffer, std::size_t length,
		      SocketAddress address) const noexcept;

#ifndef _WIN32
	void Shutdown() const noexcept;
	void ShutdownRead() const noexcept;
	void ShutdownWrite() const noexcept;
#endif
};

static_assert(std::is_trivial<SocketDescriptor>::value, "type is not trivial");

#endif
