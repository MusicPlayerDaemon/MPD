/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

#include <boost/intrusive/link_mode.hpp>
#include <boost/intrusive/list_hook.hpp>

#include <set>
#include <string>
#include <list>

#include <stddef.h>

class SocketAddress;
class EventLoop;
class Path;
struct Partition;
class Database;
class Storage;

class Client final
	: FullyBufferedSocket, TimeoutMonitor,
	  public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>> {
public:
	Partition &partition;
	struct playlist &playlist;
	struct PlayerControl &player_control;

	unsigned permission;

	/** the uid of the client process, or -1 if unknown */
	const int uid;

	CommandListBuilder cmd_list;

	const unsigned int num;	/* client number */

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

	~Client() {
		if (FullyBufferedSocket::IsDefined())
			FullyBufferedSocket::Close();
	}

	bool IsConnected() const {
		return FullyBufferedSocket::IsDefined();
	}

	gcc_pure
	bool IsExpired() const {
		return !FullyBufferedSocket::IsDefined();
	}

	void Close();
	void SetExpired();

	bool Write(const void *data, size_t length);

	/**
	 * Write a null-terminated string.
	 */
	bool Write(const char *data);

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
		return uid >= 0;
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

	/**
	 * Is this client allowed to use the specified local file?
	 *
	 * Note that this function is vulnerable to timing/symlink attacks.
	 * We cannot fix this as long as there are plugins that open a file by
	 * its name, and not by file descriptor / callbacks.
	 *
	 * Throws #std::runtime_error on error.
	 *
	 * @param path_fs the absolute path name in filesystem encoding
	 */
	void AllowFile(Path path_fs) const;

	/**
	 * Wrapper for Instance::GetDatabase().
	 */
	gcc_pure
	const Database *GetDatabase() const;

	/**
	 * Wrapper for Instance::GetDatabaseOrThrow().
	 */
	gcc_pure
	const Database &GetDatabaseOrThrow() const;

	gcc_pure
	const Storage *GetStorage() const;

private:
	/* virtual methods from class BufferedSocket */
	virtual InputResult OnSocketInput(void *data, size_t length) override;
	void OnSocketError(std::exception_ptr ep) override;
	virtual void OnSocketClosed() override;

	/* virtual methods from class TimeoutMonitor */
	virtual void OnTimeout() override;
};

void
client_manager_init();

void
client_new(EventLoop &loop, Partition &partition,
	   int fd, SocketAddress address, int uid);

/**
 * Write a printf-like formatted string to the client.
 */
gcc_printf(2,3)
void
client_printf(Client &client, const char *fmt, ...);

#endif
