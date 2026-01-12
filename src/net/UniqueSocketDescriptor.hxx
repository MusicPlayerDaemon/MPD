// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "SocketDescriptor.hxx"
#include "util/TagStructs.hxx"

#ifndef _WIN32
#include "io/UniqueFileDescriptor.hxx"
#endif

#include <utility>

class StaticSocketAddress;

/**
 * Wrapper for a socket file descriptor.
 */
class UniqueSocketDescriptor : public SocketDescriptor {
public:
	UniqueSocketDescriptor() noexcept
		:SocketDescriptor(SocketDescriptor::Undefined()) {}

	explicit UniqueSocketDescriptor(AdoptTag, SocketDescriptor _fd) noexcept
		:SocketDescriptor(_fd) {}
#ifndef _WIN32
	explicit UniqueSocketDescriptor(AdoptTag, FileDescriptor _fd) noexcept
		:SocketDescriptor(_fd) {}

	explicit UniqueSocketDescriptor(UniqueFileDescriptor &&_fd) noexcept
		:SocketDescriptor(_fd.Release()) {}
#endif // !_WIN32

	explicit UniqueSocketDescriptor(AdoptTag, int _fd) noexcept
		:SocketDescriptor(_fd) {}

#ifdef _WIN32
	UniqueSocketDescriptor(UniqueSocketDescriptor &&other) noexcept
		:SocketDescriptor(std::exchange(other.fd, INVALID_SOCKET)) {}
#else // !_WIN32
	UniqueSocketDescriptor(UniqueSocketDescriptor &&other) noexcept
		:SocketDescriptor(std::exchange(other.fd, -1)) {}
#endif // !_WIN32

	~UniqueSocketDescriptor() noexcept {
		if (IsDefined())
			Close();
	}

	/**
	 * Release ownership and return the descriptor as an unmanaged
	 * #SocketDescriptor instance.
	 */
	SocketDescriptor Release() noexcept {
		return std::exchange(*(SocketDescriptor *)this, Undefined());
	}

#ifndef _WIN32
	UniqueFileDescriptor MoveToFileDescriptor() && noexcept {
		return UniqueFileDescriptor{AdoptTag{}, Release().ToFileDescriptor()};
	}
#endif

	UniqueSocketDescriptor &operator=(UniqueSocketDescriptor &&src) noexcept {
		using std::swap;
		swap(fd, src.fd);
		return *this;
	}

	bool operator==(const UniqueSocketDescriptor &other) const noexcept {
		return fd == other.fd;
	}

	/**
	 * @return an "undefined" instance on error
	 */
	UniqueSocketDescriptor Accept() const noexcept {
		return UniqueSocketDescriptor{AdoptTag{}, SocketDescriptor::Accept()};
	}

	/**
	 * @return an "undefined" instance on error
	 */
	UniqueSocketDescriptor AcceptNonBlock() const noexcept {
		return UniqueSocketDescriptor{AdoptTag{}, SocketDescriptor::AcceptNonBlock()};
	}

	/**
	 * @return an "undefined" instance on error
	 */
	UniqueSocketDescriptor AcceptNonBlock(StaticSocketAddress &address) const noexcept {
		return UniqueSocketDescriptor{AdoptTag{}, SocketDescriptor::AcceptNonBlock(address)};
	}

#ifndef _WIN32
	static bool CreateSocketPair(int domain, int type, int protocol,
				     UniqueSocketDescriptor &a,
				     UniqueSocketDescriptor &b) noexcept {
		return SocketDescriptor::CreateSocketPair(domain, type,
							  protocol,
							  a, b);
	}

	static bool CreateSocketPairNonBlock(int domain, int type, int protocol,
					     UniqueSocketDescriptor &a,
					     UniqueSocketDescriptor &b) noexcept {
		return SocketDescriptor::CreateSocketPairNonBlock(domain, type,
								  protocol,
								  a, b);
	}
#endif
};
