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

#ifndef MPD_OGG_STREAM_STATE_HXX
#define MPD_OGG_STREAM_STATE_HXX

#include "check.h"

#include <ogg/ogg.h>

#include <assert.h>
#include <string.h>
#include <stdint.h>

class OggStreamState {
	ogg_stream_state state;

public:
	explicit OggStreamState(int serialno) {
		ogg_stream_init(&state, serialno);
	}

	~OggStreamState() {
		ogg_stream_clear(&state);
	}

	operator ogg_stream_state &() {
		return state;
	}

	void Reinitialize(int serialno) {
		ogg_stream_reset_serialno(&state, serialno);
	}

	/* encoding */

	void PacketIn(const ogg_packet &packet) {
		ogg_stream_packetin(&state,
				    const_cast<ogg_packet *>(&packet));
	}

	bool PageOut(ogg_page &page) {
		return ogg_stream_pageout(&state, &page) != 0;
	}

	bool Flush(ogg_page &page) {
		return ogg_stream_flush(&state, &page) != 0;
	}

	/* decoding */

	bool PageIn(ogg_page &page) {
		return ogg_stream_pagein(&state, &page) == 0;
	}

	int PacketOut(ogg_packet &packet) {
		return ogg_stream_packetout(&state, &packet);
	}
};

#endif
