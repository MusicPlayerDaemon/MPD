/*
 * Copyright 2003-2018 The Music Player Daemon Project
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
#include "ClientInternal.hxx"
#include "ClientList.hxx"
#include "Partition.hxx"
#include "Instance.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/SocketAddress.hxx"
#include "net/ToString.hxx"
#include "Permission.hxx"
#include "Log.hxx"

#include <assert.h>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

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
	 num(_num)
{
	timeout_event.Schedule(client_timeout);
}

void
client_new(EventLoop &loop, Partition &partition,
	   UniqueSocketDescriptor fd, SocketAddress address, int uid,
	   unsigned permission) noexcept
{
	static unsigned int next_client_num;
	const auto remote = ToString(address);

	assert(fd.IsDefined());

	ClientList &client_list = *partition.instance.client_list;
	if (client_list.IsFull()) {
		LogWarning(client_domain, "Max connections reached");
		return;
	}

	(void)fd.Write(GREETING, sizeof(GREETING) - 1);

	Client *client = new Client(loop, partition, std::move(fd), uid,
				    permission,
				    next_client_num++);

	client_list.Add(*client);

	FormatInfo(client_domain, "[%u] opened from %s",
		   client->num, remote.c_str());
}

void
Client::Close() noexcept
{
	partition->instance.client_list->Remove(*this);

	SetExpired();

	FormatInfo(client_domain, "[%u] closed", num);
	delete this;
}
