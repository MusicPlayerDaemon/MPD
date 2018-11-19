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

#ifndef MPD_CLIENT_H
#define MPD_CLIENT_H

#include "ClientMessage.hxx"
#include "command/CommandListBuilder.hxx"
#include "tag/Mask.hxx"
#include "event/FullyBufferedSocket.hxx"
#include "event/TimerEvent.hxx"
#include "util/Compiler.h"

#include <boost/intrusive/link_mode.hpp>
#include <boost/intrusive/list_hook.hpp>

#include <set>
#include <string>
#include <list>

#include <stddef.h>

struct ConfigData;
class SocketAddress;
class UniqueSocketDescriptor;
class EventLoop;
class Path;
struct Instance;
struct Partition;
class PlayerControl;
struct playlist;
class Database;
class Storage;

class Client final
	: FullyBufferedSocket,
	  public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>> {
	TimerEvent timeout_event;

	Partition *partition;

public:
	unsigned permission;

	/** the uid of the client process, or -1 if unknown */
	const int uid;

	CommandListBuilder cmd_list;

	const unsigned int num;	/* client number */

	/** is this client waiting for an "idle" response? */
	bool idle_waiting = false;

	/** idle flags pending on this client, to be sent as soon as
	    the client enters "idle" */
	unsigned idle_flags = 0;

	/** idle flags that the client wants to receive */
	unsigned idle_subscriptions;

	/**
	 * The tags this client is interested in.
	 */
	TagMask tag_mask = TagMask::All();

	/**
	 * A list of channel names this client is subscribed to.
	 */
	std::set<std::string> subscriptions;

	/**
	 * The number of subscriptions in #subscriptions.  Used to
	 * limit the number of subscriptions.
	 */
	unsigned num_subscriptions = 0;

	/**
	 * A list of messages this client has received.
	 */
	std::list<ClientMessage> messages;

	Client(EventLoop &loop, Partition &partition,
	       UniqueSocketDescriptor fd, int uid,
	       unsigned _permission,
	       int num) noexcept;

	~Client() noexcept {
		if (FullyBufferedSocket::IsDefined())
			FullyBufferedSocket::Close();
	}

	bool IsConnected() const noexcept {
		return FullyBufferedSocket::IsDefined();
	}

	gcc_pure
	bool IsExpired() const noexcept {
		return !FullyBufferedSocket::IsDefined();
	}

	void Close() noexcept;
	void SetExpired() noexcept;

	bool Write(const void *data, size_t length);

	/**
	 * Write a null-terminated string.
	 */
	bool Write(const char *data);

	/**
	 * returns the uid of the client process, or a negative value
	 * if the uid is unknown
	 */
	int GetUID() const noexcept {
		return uid;
	}

	/**
	 * Is this client running on the same machine, connected with
	 * a local (UNIX domain) socket?
	 */
	bool IsLocal() const noexcept {
		return uid >= 0;
	}

	unsigned GetPermission() const noexcept {
		return permission;
	}

	void SetPermission(unsigned _permission) noexcept {
		permission = _permission;
	}

	/**
	 * Send "idle" response to this client.
	 */
	void IdleNotify() noexcept;
	void IdleAdd(unsigned flags) noexcept;
	bool IdleWait(unsigned flags) noexcept;

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
	bool IsSubscribed(const char *channel_name) const noexcept {
		return subscriptions.find(channel_name) != subscriptions.end();
	}

	SubscribeResult Subscribe(const char *channel) noexcept;
	bool Unsubscribe(const char *channel) noexcept;
	void UnsubscribeAll() noexcept;
	bool PushMessage(const ClientMessage &msg) noexcept;

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

	Partition &GetPartition() noexcept {
		return *partition;
	}

	void SetPartition(Partition &new_partition) noexcept {
		partition = &new_partition;

		// TODO: set various idle flags?
	}

	gcc_pure
	Instance &GetInstance() noexcept;

	gcc_pure
	playlist &GetPlaylist() noexcept;

	gcc_pure
	PlayerControl &GetPlayerControl() noexcept;

	/**
	 * Wrapper for Instance::GetDatabase().
	 */
	gcc_pure
	const Database *GetDatabase() const noexcept;

	/**
	 * Wrapper for Instance::GetDatabaseOrThrow().
	 */
	const Database &GetDatabaseOrThrow() const;

	gcc_pure
	const Storage *GetStorage() const noexcept;

private:
	/* virtual methods from class BufferedSocket */
	InputResult OnSocketInput(void *data, size_t length) noexcept override;
	void OnSocketError(std::exception_ptr ep) noexcept override;
	void OnSocketClosed() noexcept override;

	/* callback for TimerEvent */
	void OnTimeout() noexcept;
};

void
client_manager_init(const ConfigData &config);

void
client_new(EventLoop &loop, Partition &partition,
	   UniqueSocketDescriptor fd, SocketAddress address, int uid,
	   unsigned permission) noexcept;

#endif
