/*
 * Copyright 2003-2016 The Music Player Daemon Project
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
#include "DecoderAPI.hxx"
#include "DecoderControl.hxx"
#include "Bridge.hxx"
#include "input/InputStream.hxx"
#include "Log.hxx"

#include <assert.h>

size_t
decoder_read(DecoderClient *client,
	     InputStream &is,
	     void *buffer, size_t length)
try {
	assert(buffer != nullptr);

	/* XXX don't allow client==nullptr */
	if (client == nullptr)
		return is.LockRead(buffer, length);

	auto &bridge = *(DecoderBridge *)client;

	assert(bridge.dc.state == DecoderState::START ||
	       bridge.dc.state == DecoderState::DECODE);

	if (length == 0)
		return 0;

	ScopeLock lock(is.mutex);

	while (true) {
		if (bridge.CheckCancelRead())
			return 0;

		if (is.IsAvailable())
			break;

		is.cond.wait(is.mutex);
	}

	size_t nbytes = is.Read(buffer, length);
	assert(nbytes > 0 || is.IsEOF());

	return nbytes;
} catch (const std::runtime_error &e) {
	auto *bridge = (DecoderBridge *)client;
	if (bridge != nullptr)
		bridge->error = std::current_exception();
	else
		LogError(e);
	return 0;
}

bool
decoder_read_full(DecoderClient *client, InputStream &is,
		  void *_buffer, size_t size)
{
	uint8_t *buffer = (uint8_t *)_buffer;

	while (size > 0) {
		size_t nbytes = decoder_read(client, is, buffer, size);
		if (nbytes == 0)
			return false;

		buffer += nbytes;
		size -= nbytes;
	}

	return true;
}

bool
decoder_skip(DecoderClient *client, InputStream &is, size_t size)
{
	while (size > 0) {
		char buffer[1024];
		size_t nbytes = decoder_read(client, is, buffer,
					     std::min(sizeof(buffer), size));
		if (nbytes == 0)
			return false;

		size -= nbytes;
	}

	return true;
}
