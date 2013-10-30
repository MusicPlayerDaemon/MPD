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

#ifndef MPD_CLIENT_H
#define MPD_CLIENT_H

#include "check.h"
#include "ClientMessage.hxx"
#include "command/CommandListBuilder.hxx"
#include "event/FullyBufferedSocket.hxx"
#include "event/TimeoutMonitor.hxx"
#include "Compiler.h"

#include <set>
#include <string>
#include <list>

#include <stddef.h>
#include <stdarg.h>

struct sockaddr;
class EventLoop;
struct Partition;

class Client final : private FullyBufferedSocket, TimeoutMonitor {
public:
	Partition &partition;
	struct playlist &playlist;
	struct PlayerControl &player_control;

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
	bool IsExpired() const {
		return !FullyBufferedSocket::IsDefined();
	}

	void Close();
	void SetExpired();

	using FullyBufferedSocket::Write;

	/**
	 * returns the uid of the client process, or a negative value
	 * if the uid is unknown
	 */
	int GetUID() const {
		return uid;
	}

	/**
	 * Is this client running on the same machine, connected with
	 * a local (UNIX domain) socket?
	 */
	bool IsLocal() const {
		return uid > 0;
	}

	unsigned GetPermission() const {
		return permission;
	}

	void SetPermission(unsigned _permission) {
		permission = _permission;
	}

	/**
	 * Send "idle" response to this client.
	 */
	void IdleNotify();
	void IdleAdd(unsigned flags);
	bool IdleWait(unsigned flags);

	enum class SubscribeResult {
		/** success */
		OK,

		/** invalid channel name */
		INVALID,

		/** already subscribed to this channel */
		ALREADY,

		/** too many subscriptions */
		FULL,
	};

	gcc_pure
	bool IsSubscribed(const char *channel_name) const {
		return subscriptions.find(channel_name) != subscriptions.end();
	}

	SubscribeResult Subscribe(const char *channel);
	bool Unsubscribe(const char *channel);
	void UnsubscribeAll();
	bool PushMessage(const ClientMessage &msg);

private:
	/* virtual methods from class BufferedSocket */
	virtual InputResult OnSocketInput(void *data, size_t length) override;
	virtual void OnSocketError(Error &&error) override;
	virtual void OnSocketClosed() override;

	/* virtual methods from class TimeoutMonitor */
	virtual void OnTimeout() override;
};

void client_manager_init(void);

void
client_new(EventLoop &loop, Partition &partition,
	   int fd, const struct sockaddr *sa, size_t sa_length, int uid);

/**
 * Write a C string to the client.
 */
void client_puts(Client &client, const char *s);

/**
 * Write a printf-like formatted string to the client.
 */
void client_vprintf(Client &client, const char *fmt, va_list args);

/**
 * Write a printf-like formatted string to the client.
 */
gcc_printf(2,3)
void
client_printf(Client &client, const char *fmt, ...);

#endif
