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
#include "net/PeerCredentials.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/SocketAddress.hxx"
#include "net/ToString.hxx"
#include "util/SpanCast.hxx"
#include "Log.hxx"
#include "Version.h"

#include <fmt/core.h>

#include <cassert>

using std::string_view_literals::operator""sv;

static constexpr auto GREETING = "OK MPD " PROTOCOL_VERSION "\n"sv;

Client::Client(EventLoop &_loop, Partition &_partition,
	       UniqueSocketDescriptor _fd,
	       int _uid, unsigned _permission,
	       std::string &&_name) noexcept
	:FullyBufferedSocket(_fd.Release(), _loop,
			     16384, client_max_output_buffer_size),
	 name(std::move(_name)),
	 timeout_event(_loop, BIND_THIS_METHOD(OnTimeout)),
	 partition(&_partition),
	 permission(_permission),
	 uid(_uid),
	 last_album_art(_loop)
{
	FmtInfo(client_domain, "[{}] client connected", name);

	timeout_event.Schedule(client_timeout);
}

[[gnu::pure]]
static std::string
MakeClientName(SocketAddress address, const SocketPeerCredentials &cred) noexcept
{
	if (cred.IsDefined()) {
		if (cred.GetPid() > 0)
			return fmt::format("pid={} uid={}", cred.GetPid(), cred.GetUid());

		return fmt::format("uid={}", cred.GetUid());
	}

	return ToString(address);
}

void
client_new(EventLoop &loop, Partition &partition,
	   UniqueSocketDescriptor fd, SocketAddress remote_address,
	   SocketPeerCredentials cred,
	   unsigned permission) noexcept
{
	assert(fd.IsDefined());

	ClientList &client_list = *partition.instance.client_list;
	if (client_list.IsFull()) {
		LogWarning(client_domain, "Max connections reached");
		return;
	}

	(void)fd.WriteNoWait(AsBytes(GREETING));

	const int uid = cred.IsDefined() ? static_cast<int>(cred.GetUid()) : -1;

	auto *client = new Client(loop, partition, std::move(fd), uid,
				  permission,
				  MakeClientName(remote_address, cred));

	client_list.Add(*client);
	partition.clients.push_back(*client);
}

void
Client::Close() noexcept
{
	partition->instance.client_list->Remove(*this);
	partition->clients.erase(partition->clients.iterator_to(*this));

	if (FullyBufferedSocket::IsDefined())
		FullyBufferedSocket::Close();

	FmtInfo(client_domain, "[{}] disconnected", name);
	delete this;
}
