// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/** \file
 *
 * Internal declarations for the "httpd" audio output plugin.
 */

#ifndef MPD_OUTPUT_HTTPD_INTERNAL_H
#define MPD_OUTPUT_HTTPD_INTERNAL_H

#include "HttpdClient.hxx"
#include "output/Interface.hxx"
#include "output/Timer.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "event/ServerSocket.hxx"
#include "event/InjectEvent.hxx"
#include "util/Cast.hxx"
#include "util/IntrusiveList.hxx"

#include <queue>
#include <list>
#include <memory>
#include <span>

struct ConfigBlock;
class EventLoop;
class ServerSocket;
class HttpdClient;
class PreparedEncoder;
class Encoder;
struct Tag;

class HttpdOutput final : AudioOutput, ServerSocket {
	/**
	 * True if the audio output is open and accepts client
	 * connections.
	 */
	bool open;

	bool pause;

	/**
	 * The configured encoder plugin.
	 */
	std::unique_ptr<PreparedEncoder> prepared_encoder;
	Encoder *encoder = nullptr;

	/**
	 * Number of bytes which were fed into the encoder, without
	 * ever receiving new output.  This is used to estimate
	 * whether MPD should manually flush the encoder, to avoid
	 * buffer underruns in the client.
	 */
	size_t unflushed_input = 0;

public:
	/**
	 * The MIME type produced by the #encoder.
	 */
	const char *content_type;

	/**
	 * This mutex protects the listener socket and the client
	 * list.
	 */
	mutable Mutex mutex;

	/**
	 * This condition gets signalled when an item is removed from
	 * #pages.
	 */
	Cond cond;

private:
	/**
	 * A #Timer object to synchronize this output with the
	 * wallclock.
	 */
	Timer *timer;

	/**
	 * The header page, which is sent to every client on connect.
	 */
	PagePtr header;

	/**
	 * The metadata, which is sent to every client.
	 */
	PagePtr metadata;

	/**
	 * The page queue, i.e. pages from the encoder to be
	 * broadcasted to all clients.  This container is necessary to
	 * pass pages from the OutputThread to the IOThread.  It is
	 * protected by #mutex, and removing signals #cond.
	 */
	std::queue<PagePtr, std::list<PagePtr>> pages;

	InjectEvent defer_broadcast;

 public:
	/**
	 * The configured name.
	 */
	char const *const name;

	/**
	 * The configured genre.
	 */
	char const *const genre;

	/**
	 * The configured website address.
	 */
	char const *const website;

private:
	/**
	 * A linked list containing all clients which are currently
	 * connected.
	 */
	IntrusiveList<HttpdClient, IntrusiveListBaseHookTraits<HttpdClient>,
		      true> clients;

	/**
	 * The maximum number of clients connected at the same time.
	 */
	const unsigned clients_max;

public:
	HttpdOutput(EventLoop &_loop, const ConfigBlock &block);

	static AudioOutput *Create(EventLoop &event_loop,
				   const ConfigBlock &block) {
		return new HttpdOutput(event_loop, block);
	}

	using ServerSocket::GetEventLoop;

	void Bind();
	void Unbind() noexcept;

	void Enable() override {
		Bind();
	}

	void Disable() noexcept override {
		Unbind();
	}

	/**
	 * Caller must lock the mutex.
	 *
	 * Throws on error.
	 */
	void OpenEncoder(AudioFormat &audio_format);

	/**
	 * Caller must lock the mutex.
	 */
	void Open(AudioFormat &audio_format) override;

	/**
	 * Caller must lock the mutex.
	 */
	void Close() noexcept override;

	/**
	 * Check whether there is at least one client.
	 *
	 * Caller must lock the mutex.
	 */
	[[gnu::pure]]
	bool HasClients() const noexcept {
		return !clients.empty();
	}

	/**
	 * Check whether there is at least one client.
	 */
	[[gnu::pure]]
	bool LockHasClients() const noexcept {
		const std::scoped_lock<Mutex> protect(mutex);
		return HasClients();
	}

	/**
	 * Caller must lock the mutex.
	 */
	void AddClient(UniqueSocketDescriptor fd) noexcept;

	/**
	 * Removes a client from the httpd_output.clients linked list.
	 *
	 * Caller must lock the mutex.
	 */
	void RemoveClient(HttpdClient &client) noexcept;

	/**
	 * Sends the encoder header to the client.  This is called
	 * right after the response headers have been sent.
	 */
	void SendHeader(HttpdClient &client) const noexcept;

	[[gnu::pure]]
	std::chrono::steady_clock::duration Delay() const noexcept override;

	/**
	 * Reads data from the encoder (as much as available) and
	 * returns it as a new #page object.
	 */
	PagePtr ReadPage() noexcept;

	/**
	 * Broadcasts a page struct to all clients.
	 *
	 * Mutext must not be locked.
	 */
	void BroadcastPage(PagePtr page) noexcept;

	/**
	 * Broadcasts data from the encoder to all clients.
	 *
	 * Mutext must not be locked.
	 */
	void BroadcastFromEncoder() noexcept;

	/**
	 * Mutext must not be locked.
	 *
	 * Throws on error.
	 */
	void EncodeAndPlay(std::span<const std::byte> src);

	void SendTag(const Tag &tag) override;

	std::size_t Play(std::span<const std::byte> src) override;

	/**
	 * Mutext must not be locked.
	 */
	void CancelAllClients() noexcept;

	void Cancel() noexcept override;
	bool Pause() override;

private:
	/* InjectEvent callback */
	void OnDeferredBroadcast() noexcept;

	void OnAccept(UniqueSocketDescriptor fd,
		      SocketAddress address, int uid) noexcept override;
};

extern const class Domain httpd_output_domain;

#endif
