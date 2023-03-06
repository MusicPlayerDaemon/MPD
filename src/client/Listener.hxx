// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_CLIENT_LISTENER_HXX
#define MPD_CLIENT_LISTENER_HXX

#include "event/ServerSocket.hxx"

struct Partition;

class ClientListener final : public ServerSocket {
	Partition &partition;

public:
	ClientListener(EventLoop &_loop, Partition &_partition) noexcept
		:ServerSocket(_loop), partition(_partition) {}

private:
	void OnAccept(UniqueSocketDescriptor fd,
		      SocketAddress address, int uid) noexcept override;
};

#endif
