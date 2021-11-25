/*
 * Copyright 2012-2019 Max Kellermann <max.kellermann@gmail.com>
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
