// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "IClient.hxx"
#include "Message.hxx"
#include "ProtocolFeature.hxx"
#include "command/CommandResult.hxx"
#include "command/CommandListBuilder.hxx"
#include "input/LastInputStream.hxx"
#include "tag/Mask.hxx"
#include "event/FullyBufferedSocket.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "util/IntrusiveList.hxx"

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
	: public IClient, FullyBufferedSocket
{
	friend struct ClientPerPartitionListHook;
	friend class ClientList;

	IntrusiveListHook<> list_siblings, partition_siblings;

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
	std::set<std::string, std::less<>> subscriptions;

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

	/**
	 * Bitmask of protocol features.
	 */
	ProtocolFeature protocol_feature = ProtocolFeature::None();

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

	ProtocolFeature GetProtocolFeatures() const noexcept {
		return protocol_feature;
	}

	void SetProtocolFeatures(ProtocolFeature features, bool enable) noexcept {
		if (enable)
			protocol_feature.Set(features);
		else
			protocol_feature.Unset(features);
	}

	void AllProtocolFeatures() noexcept {
		protocol_feature.SetAll();
	}

	void ClearProtocolFeatures() noexcept {
		protocol_feature.Clear();
	}

	bool ProtocolFeatureEnabled(enum ProtocolFeatureType value) noexcept {
		return protocol_feature.Test(value);
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
	 * Wrapper for Instance::GetDatabaseOrThrow().
	 */
	const Database &GetDatabaseOrThrow() const;

	// virtual methods from class IClient
	void AllowFile(Path path_fs) const override;

#ifdef ENABLE_DATABASE
	const Database *GetDatabase() const noexcept override;
	Storage *GetStorage() const noexcept override;
#endif // ENABLE_DATABASE

private:
	CommandResult ProcessCommandList(bool list_ok,
					 std::list<std::string> &&list) noexcept;

	CommandResult ProcessLine(char *line) noexcept;

	/* virtual methods from class BufferedSocket */
	InputResult OnSocketInput(std::span<std::byte> src) noexcept override;
	void OnSocketError(std::exception_ptr ep) noexcept override;
	void OnSocketClosed() noexcept override;

	/* callback for TimerEvent */
	void OnTimeout() noexcept;
};

struct ClientPerPartitionListHook
	: IntrusiveListMemberHookTraits<&Client::partition_siblings> {};

void
client_new(EventLoop &loop, Partition &partition,
	   UniqueSocketDescriptor fd, SocketAddress address, int uid,
	   unsigned permission) noexcept;
