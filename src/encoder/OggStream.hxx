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

#ifndef MPD_OGG_STREAM_HXX
#define MPD_OGG_STREAM_HXX

#include "check.h"

#include <ogg/ogg.h>

#include <assert.h>
#include <string.h>
#include <stdint.h>

class OggStream {
	ogg_stream_state state;

	bool flush;

#ifndef NDEBUG
	bool initialized;
#endif

public:
#ifndef NDEBUG
	OggStream():initialized(false) {}
	~OggStream() {
		assert(!initialized);
	}
#endif

	void Initialize(int serialno) {
		assert(!initialized);

		ogg_stream_init(&state, serialno);

		/* set "flush" to true, so the caller gets the full
		   headers on the first read() */
		flush = true;

#ifndef NDEBUG
		initialized = true;
#endif
	}

	void Reinitialize(int serialno) {
		assert(initialized);

		ogg_stream_reset_serialno(&state, serialno);

		/* set "flush" to true, so the caller gets the full
		   headers on the first read() */
		flush = true;
	}

	void Deinitialize() {
		assert(initialized);

		ogg_stream_clear(&state);

#ifndef NDEBUG
		initialized = false;
#endif
	}

	void Flush() {
		assert(initialized);

		flush = true;
	}

	void PacketIn(const ogg_packet &packet) {
		assert(initialized);

		ogg_stream_packetin(&state,
				    const_cast<ogg_packet *>(&packet));
	}

	bool PageOut(ogg_page &page) {
		int result = ogg_stream_pageout(&state, &page);
		if (result == 0 && flush) {
			flush = false;
			result = ogg_stream_flush(&state, &page);
		}

		return result != 0;
	}

	size_t PageOut(void *_buffer, size_t size) {
		ogg_page page;
		if (!PageOut(page))
			return 0;

		assert(page.header_len > 0 || page.body_len > 0);

		size_t header_len = (size_t)page.header_len;
		size_t body_len = (size_t)page.body_len;
		assert(header_len <= size);

		if (header_len + body_len > size)
			/* TODO: better overflow handling */
			body_len = size - header_len;

		uint8_t *buffer = (uint8_t *)_buffer;
		memcpy(buffer, page.header, header_len);
		memcpy(buffer + header_len, page.body, body_len);

		return header_len + body_len;
	}
};

#endif
