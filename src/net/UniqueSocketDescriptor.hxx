// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef UNIQUE_SOCKET_DESCRIPTOR_SOCKET_HXX
#define UNIQUE_SOCKET_DESCRIPTOR_SOCKET_HXX

#include "SocketDescriptor.hxx"

#include <utility>

class StaticSocketAddress;

/**
 * Wrapper for a socket file descriptor.
 */
class UniqueSocketDescriptor : public SocketDescriptor {
public:
	UniqueSocketDescriptor() noexcept
		:SocketDescriptor(SocketDescriptor::Undefined()) {}

	explicit UniqueSocketDescriptor(SocketDescriptor _fd) noexcept
		:SocketDescriptor(_fd) {}
	explicit UniqueSocketDescriptor(FileDescriptor _fd) noexcept
		:SocketDescriptor(_fd) {}
	explicit UniqueSocketDescriptor(int _fd) noexcept
		:SocketDescriptor(_fd) {}

	UniqueSocketDescriptor(UniqueSocketDescriptor &&other) noexcept
		:SocketDescriptor(std::exchange(other.fd, -1)) {}

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
	UniqueSocketDescriptor AcceptNonBlock() const noexcept {
		return UniqueSocketDescriptor(SocketDescriptor::AcceptNonBlock());
	}

	/**
	 * @return an "undefined" instance on error
	 */
	UniqueSocketDescriptor AcceptNonBlock(StaticSocketAddress &address) const noexcept {
		return UniqueSocketDescriptor(SocketDescriptor::AcceptNonBlock(address));
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

#endif
