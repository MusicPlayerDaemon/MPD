/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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

#ifndef MPD_CLIENT_INTERNAL_HXX
#define MPD_CLIENT_INTERNAL_HXX

#include "check.h"
#include "Client.hxx"
#include "ClientMessage.hxx"
#include "CommandListBuilder.hxx"
#include "event/FullyBufferedSocket.hxx"
#include "event/TimeoutMonitor.hxx"
#include "command.h"

#include <set>
#include <string>
#include <list>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "client"

enum {
	CLIENT_MAX_SUBSCRIPTIONS = 16,
	CLIENT_MAX_MESSAGES = 64,
};

struct Partition;

class Client final : private FullyBufferedSocket, TimeoutMonitor {
public:
	Partition &partition;
	struct playlist &playlist;
	struct player_control *player_control;

	unsigned permission;

	/** the uid of the client process, or -1 if unknown */
	int uid;

	CommandListBuilder cmd_list;

	unsigned int num;	/* client number */

	/** is this client waiting for an "idle" response? */
	bool idle_waiting;

	/** idle flags pending on this client, to be sent as soon as
	    the client enters "idle" */
	unsigned idle_flags;

	/** idle flags that the client wants to receive */
	unsigned idle_subscriptions;

	/**
	 * A list of channel names this client is subscribed to.
	 */
	std::set<std::string> subscriptions;

	/**
	 * The number of subscriptions in #subscriptions.  Used to
	 * limit the number of subscriptions.
	 */
	unsigned num_subscriptions;

	/**
	 * A list of messages this client has received.
	 */
	std::list<ClientMessage> messages;

	Client(EventLoop &loop, Partition &partition,
	       int fd, int uid, int num);

	bool IsConnected() const {
		return FullyBufferedSocket::IsDefined();
	}

	gcc_pure
	bool IsSubscribed(const char *channel_name) const {
		return subscriptions.find(channel_name) != subscriptions.end();
	}

	gcc_pure
	bool IsExpired() const {
		return !FullyBufferedSocket::IsDefined();
	}

	void Close();
	void SetExpired();

	using FullyBufferedSocket::Write;

	/**
	 * Send "idle" response to this client.
	 */
	void IdleNotify();
	void IdleAdd(unsigned flags);
	bool IdleWait(unsigned flags);

private:
	/* virtual methods from class BufferedSocket */
	virtual InputResult OnSocketInput(const void *data,
					  size_t length) override;
	virtual void OnSocketError(GError *error) override;
	virtual void OnSocketClosed() override;

	/* virtual methods from class TimeoutMonitor */
	virtual void OnTimeout() override;
};

extern int client_timeout;
extern size_t client_max_command_list_size;
extern size_t client_max_output_buffer_size;

enum command_return
client_process_line(Client *client, char *line);

#endif
