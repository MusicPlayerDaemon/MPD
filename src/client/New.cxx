// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Client.hxx"
#include "Config.hxx"
#include "Domain.hxx"
#include "List.hxx"
#include "BackgroundCommand.hxx"
#include "Partition.hxx"
#include "Instance.hxx"
#include "lib/fmt/SocketAddressFormatter.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/SocketAddress.hxx"
#include "Log.hxx"
#include "Version.h"

#include <cassert>

static constexpr char GREETING[] = "OK MPD " PROTOCOL_VERSION "\n";

Client::Client(EventLoop &_loop, Partition &_partition,
	       UniqueSocketDescriptor _fd,
	       int _uid, unsigned _permission,
	       int _num) noexcept
	:FullyBufferedSocket(_fd.Release(), _loop,
			     16384, client_max_output_buffer_size),
	 timeout_event(_loop, BIND_THIS_METHOD(OnTimeout)),
	 partition(&_partition),
	 permission(_permission),
	 uid(_uid),
	 num(_num),
	 last_album_art(_loop)
{
	timeout_event.Schedule(client_timeout);
}

void
client_new(EventLoop &loop, Partition &partition,
	   UniqueSocketDescriptor fd, SocketAddress remote_address, int uid,
	   unsigned permission) noexcept
{
	static unsigned int next_client_num;

	assert(fd.IsDefined());

	ClientList &client_list = *partition.instance.client_list;
	if (client_list.IsFull()) {
		LogWarning(client_domain, "Max connections reached");
		return;
	}

	(void)fd.Write(GREETING, sizeof(GREETING) - 1);

	const unsigned num = next_client_num++;
	auto *client = new Client(loop, partition, std::move(fd), uid,
				    permission,
				    num);

	client_list.Add(*client);
	partition.clients.push_back(*client);

	FmtInfo(client_domain, "[{}] opened from {}",
		num, remote_address);
}

void
Client::Close() noexcept
{
	partition->instance.client_list->Remove(*this);
	partition->clients.erase(partition->clients.iterator_to(*this));

	if (FullyBufferedSocket::IsDefined())
		FullyBufferedSocket::Close();

	FmtInfo(client_domain, "[{}] closed", num);
	delete this;
}
