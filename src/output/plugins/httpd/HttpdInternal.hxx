/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

/** \file
 *
 * Internal declarations for the "httpd" audio output plugin.
 */

#ifndef MPD_OUTPUT_HTTPD_INTERNAL_H
#define MPD_OUTPUT_HTTPD_INTERNAL_H

#include "output/Internal.hxx"
#include "output/Timer.hxx"
#include "thread/Mutex.hxx"
#include "event/ServerSocket.hxx"
#include "event/DeferredMonitor.hxx"
#include "util/Cast.hxx"
#include "Compiler.h"

#ifdef _LIBCPP_VERSION
/* can't use incomplete template arguments with libc++ */
#include "HttpdClient.hxx"
#endif

#include <forward_list>
#include <queue>
#include <list>

struct config_param;
class Error;
class EventLoop;
class ServerSocket;
class HttpdClient;
class Page;
struct Encoder;
struct Tag;

class HttpdOutput final : ServerSocket, DeferredMonitor {
	AudioOutput base;

	/**
	 * True if the audio output is open and accepts client
	 * connections.
	 */
	bool open;

	/**
	 * The configured encoder plugin.
	 */
	Encoder *encoder;

	/**
	 * Number of bytes which were fed into the encoder, without
	 * ever receiving new output.  This is used to estimate
	 * whether MPD should manually flush the encoder, to avoid
	 * buffer underruns in the client.
	 */
	size_t unflushed_input;

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
	Page *header;

	/**
	 * The metadata, which is sent to every client.
	 */
	Page *metadata;

	/**
	 * The page queue, i.e. pages from the encoder to be
	 * broadcasted to all clients.  This container is necessary to
	 * pass pages from the OutputThread to the IOThread.  It is
	 * protected by #mutex, and removing signals #cond.
	 */
	std::queue<Page *, std::list<Page *>> pages;

 public:
	/**
	 * The configured name.
	 */
	char const *name;
	/**
	 * The configured genre.
	 */
	char const *genre;
	/**
	 * The configured website address.
	 */
	char const *website;

private:
	/**
	 * A linked list containing all clients which are currently
	 * connected.
	 */
	std::forward_list<HttpdClient> clients;

	/**
	 * A temporary buffer for the httpd_output_read_page()
	 * function.
	 */
	char buffer[32768];

	/**
	 * The maximum and current number of clients connected
	 * at the same time.
	 */
	unsigned clients_max, clients_cnt;

public:
	HttpdOutput(EventLoop &_loop);
	~HttpdOutput();

#if defined(__clang__) || GCC_CHECK_VERSION(4,7)
	constexpr
#endif
	static HttpdOutput *Cast(AudioOutput *ao) {
		return &ContainerCast(*ao, &HttpdOutput::base);
	}

	using DeferredMonitor::GetEventLoop;

	bool Init(const config_param &param, Error &error);

	bool Configure(const config_param &param, Error &error);

	AudioOutput *InitAndConfigure(const config_param &param,
				       Error &error) {
		if (!Init(param, error))
			return nullptr;

		if (!Configure(param, error))
			return nullptr;

		return &base;
	}

	bool Bind(Error &error);
	void Unbind();

	/**
	 * Caller must lock the mutex.
	 */
	bool OpenEncoder(AudioFormat &audio_format, Error &error);

	/**
	 * Caller must lock the mutex.
	 */
	bool Open(AudioFormat &audio_format, Error &error);

	/**
	 * Caller must lock the mutex.
	 */
	void Close();

	/**
	 * Check whether there is at least one client.
	 *
	 * Caller must lock the mutex.
	 */
	gcc_pure
	bool HasClients() const {
		return !clients.empty();
	}

	/**
	 * Check whether there is at least one client.
	 */
	gcc_pure
	bool LockHasClients() const {
		const ScopeLock protect(mutex);
		return HasClients();
	}

	void AddClient(int fd);

	/**
	 * Removes a client from the httpd_output.clients linked list.
	 */
	void RemoveClient(HttpdClient &client);

	/**
	 * Sends the encoder header to the client.  This is called
	 * right after the response headers have been sent.
	 */
	void SendHeader(HttpdClient &client) const;

	gcc_pure
	unsigned Delay() const;

	/**
	 * Reads data from the encoder (as much as available) and
	 * returns it as a new #page object.
	 */
	Page *ReadPage();

	/**
	 * Broadcasts a page struct to all clients.
	 *
	 * Mutext must not be locked.
	 */
	void BroadcastPage(Page *page);

	/**
	 * Broadcasts data from the encoder to all clients.
	 */
	void BroadcastFromEncoder();

	bool EncodeAndPlay(const void *chunk, size_t size, Error &error);

	void SendTag(const Tag *tag);

	size_t Play(const void *chunk, size_t size, Error &error);

	void CancelAllClients();

private:
	virtual void RunDeferred() override;

	virtual void OnAccept(int fd, const sockaddr &address,
			      size_t address_length, int uid) override;
};

extern const class Domain httpd_output_domain;

#endif
