/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "Message.hxx"
#include "command/CommandResult.hxx"
#include "command/CommandListBuilder.hxx"
#include "input/LastInputStream.hxx"
#include "tag/Mask.hxx"
#include "event/FullyBufferedSocket.hxx"
#include "event/CoarseTimerEvent.hxx"

#include <boost/intrusive/link_mode.hpp>
#include <boost/intrusive/list_hook.hpp>

#include <cstddef>
#include <list>
#include <memory>
#include <set>
#include <string>

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
class BackgroundCommand;

class Client final
	: FullyBufferedSocket,
	  public boost::intrusive::list_base_hook<boost::intrusive::tag<Partition>,
						  boost::intrusive::link_mode<boost::intrusive::normal_link>>,
	  public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>> {
	CoarseTimerEvent timeout_event;

	Partition *partition;

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

public:
	// TODO: make this attribute "private"
	/**
	 * The tags this client is interested in.
	 */
	TagMask tag_mask = TagMask::All();

	/**
	 * The maximum number of bytes transmitted in a binary
	 * response.  Can be changed with the "binarylimit" command.
	 */
	size_t binary_limit = 8192;

	/**
	 * This caches the last "albumart" InputStream instance, to
	 * avoid repeating the search for each chunk requested by this
	 * client.
	 */
	LastInputStream last_album_art;

private:
	static constexpr size_t MAX_SUBSCRIPTIONS = 16;

	/**
	 * A list of channel names this client is subscribed to.
	 */
	std::set<std::string> subscriptions;

	/**
	 * The number of subscriptions in #subscriptions.  Used to
	 * limit the number of subscriptions.
	 */
	unsigned num_subscriptions = 0;

	static constexpr size_t MAX_MESSAGES = 64;

	/**
	 * A list of messages this client has received.
	 */
	std::list<ClientMessage> messages;

	/**
	 * The command currently running in background.  If this is
	 * set, then the client is occupied and will not process any
	 * new input.  If the connection gets closed, the
	 * #BackgroundCommand will be cancelled.
	 */
	std::unique_ptr<BackgroundCommand> background_command;

public:
	Client(EventLoop &loop, Partition &partition,
	       UniqueSocketDescriptor fd, int uid,
	       unsigned _permission,
	       int num) noexcept;

	~Client() noexcept;

	using FullyBufferedSocket::GetEventLoop;
	using FullyBufferedSocket::GetOutputMaxSize;

	[[gnu::pure]]
	bool IsExpired() const noexcept {
		return !FullyBufferedSocket::IsDefined();
	}

	void Close() noexcept;
	void SetExpired() noexcept;

	bool Write(const void *data, size_t length) noexcept;

	/**
	 * Write a null-terminated string.
	 */
	bool Write(std::string_view s) noexcept {
		return Write(s.data(), s.size());
	}

	bool WriteOK() noexcept {
		return Write("OK\n");
	}

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

	/**
	 * Called by a command handler to defer execution to a
	 * #BackgroundCommand.
	 */
	void SetBackgroundCommand(std::unique_ptr<BackgroundCommand> _bc) noexcept;

	/**
	 * Called by the current #BackgroundCommand when it has
	 * finished, after sending the response.  This method then
	 * deletes the #BackgroundCommand.
	 */
	void OnBackgroundCommandFinished() noexcept;

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

	[[gnu::pure]]
	bool IsSubscribed(const char *channel_name) const noexcept {
		return subscriptions.find(channel_name) != subscriptions.end();
	}

	const auto &GetSubscriptions() const noexcept {
		return subscriptions;
	}

	SubscribeResult Subscribe(const char *channel) noexcept;
	bool Unsubscribe(const char *channel) noexcept;
	void UnsubscribeAll() noexcept;
	bool PushMessage(const ClientMessage &msg) noexcept;

	template<typename F>
	void ConsumeMessages(F &&f) {
		while (!messages.empty()) {
			f(messages.front());
			messages.pop_front();
		}
	}

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

	Partition &GetPartition() const noexcept {
		return *partition;
	}

	void SetPartition(Partition &new_partition) noexcept;

	[[gnu::pure]]
	Instance &GetInstance() const noexcept;

	[[gnu::pure]]
	playlist &GetPlaylist() const noexcept;

	[[gnu::pure]]
	PlayerControl &GetPlayerControl() const noexcept;

	/**
	 * Wrapper for Instance::GetDatabase().
	 */
	[[gnu::pure]]
	const Database *GetDatabase() const noexcept;

	/**
	 * Wrapper for Instance::GetDatabaseOrThrow().
	 */
	const Database &GetDatabaseOrThrow() const;

	[[gnu::pure]]
	const Storage *GetStorage() const noexcept;

private:
	CommandResult ProcessCommandList(bool list_ok,
					 std::list<std::string> &&list) noexcept;

	CommandResult ProcessLine(char *line) noexcept;

	/* virtual methods from class BufferedSocket */
	InputResult OnSocketInput(void *data, size_t length) noexcept override;
	void OnSocketError(std::exception_ptr ep) noexcept override;
	void OnSocketClosed() noexcept override;

	/* callback for TimerEvent */
	void OnTimeout() noexcept;
};

void
client_new(EventLoop &loop, Partition &partition,
	   UniqueSocketDescriptor fd, SocketAddress address, int uid,
	   unsigned permission) noexcept;

#endif
