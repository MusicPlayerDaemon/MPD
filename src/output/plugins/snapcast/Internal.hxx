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

#ifndef MPD_OUTPUT_SNAPCAST_INTERNAL_HXX
#define MPD_OUTPUT_SNAPCAST_INTERNAL_HXX

#include "Chunk.hxx"
#include "output/Interface.hxx"
#include "output/Timer.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "event/ServerSocket.hxx"
#include "event/InjectEvent.hxx"
#include "util/AllocatedArray.hxx"
#include "util/IntrusiveList.hxx"

#include "config.h" // for HAVE_ZEROCONF

#include <memory>

struct ConfigBlock;
class SnapcastClient;
class PreparedEncoder;
class Encoder;
class ZeroconfHelper;

class SnapcastOutput final : AudioOutput, ServerSocket {
#ifdef HAVE_ZEROCONF
	unsigned zeroconf_port = 0;
#endif

	/**
	 * True if the audio output is open and accepts client
	 * connections.
	 */
	bool open;

	/**
	 * Is the output current paused?  This is set by Pause() and
	 * is cleared by the next Play() call.  It is used in Delay().
	 */
	bool pause;

	InjectEvent inject_event;

#ifdef HAVE_ZEROCONF
	std::unique_ptr<ZeroconfHelper> zeroconf_helper;
#endif

	/**
	 * The configured encoder plugin.
	 */
	std::unique_ptr<PreparedEncoder> prepared_encoder;
	Encoder *encoder = nullptr;

	AllocatedArray<std::byte> codec_header;

	/**
	 * Number of bytes which were fed into the encoder, without
	 * ever receiving new output.  This is used to estimate
	 * whether MPD should manually flush the encoder, to avoid
	 * buffer underruns in the client.
	 */
	size_t unflushed_input = 0;

	/**
	 * A #Timer object to synchronize this output with the
	 * wallclock.
	 */
	Timer *timer;

	/**
	 * A linked list containing all clients which are currently
	 * connected.
	 */
	IntrusiveList<SnapcastClient> clients;

	SnapcastChunkQueue chunks;

public:
	/**
	 * This mutex protects the listener socket, the #clients list
	 * and the #chunks queue.
	 */
	mutable Mutex mutex;

	/**
	 * This cond is signalled when a #SnapcastClient has an empty
	 * queue.
	 */
	Cond drain_cond;

	SnapcastOutput(EventLoop &_loop, const ConfigBlock &block);
	~SnapcastOutput() noexcept override;

	static AudioOutput *Create(EventLoop &event_loop,
				   const ConfigBlock &block) {
		return new SnapcastOutput(event_loop, block);
	}

	using ServerSocket::GetEventLoop;

	void Bind();
	void Unbind() noexcept;

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
	 * Removes a client from the snapcast_output.clients linked list.
	 *
	 * Caller must lock the mutex.
	 */
	void RemoveClient(SnapcastClient &client) noexcept;

	/**
	 * Caller must lock the mutex.
	 *
	 * Throws on error.
	 */
	void OpenEncoder(AudioFormat &audio_format);

	const char *GetCodecName() const noexcept {
		return "pcm";
	}

	ConstBuffer<void> GetCodecHeader() const noexcept {
		ConstBuffer<std::byte> result(codec_header);
		return result.ToVoid();
	}

	/* virtual methods from class AudioOutput */
	void Enable() override {
		Bind();
	}

	void Disable() noexcept override {
		Unbind();
	}

	void Open(AudioFormat &audio_format) override;
	void Close() noexcept override;

	// TODO: void Interrupt() noexcept override;

	std::chrono::steady_clock::duration Delay() const noexcept override;

	void SendTag(const Tag &tag) override;

	size_t Play(const void *chunk, size_t size) override;

	void Drain() override;
	void Cancel() noexcept override;
	bool Pause() override;

private:
	void OnInject() noexcept;

	/**
	 * Caller must lock the mutex.
	 */
	[[gnu::pure]]
	bool IsDrained() const noexcept;

	/* virtual methods from class ServerSocket */
	void OnAccept(UniqueSocketDescriptor fd,
		      SocketAddress address, int uid) noexcept override;
};

#endif
