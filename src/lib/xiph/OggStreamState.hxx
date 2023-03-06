// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_OGG_STREAM_STATE_HXX
#define MPD_OGG_STREAM_STATE_HXX

#include <ogg/ogg.h>

#include <cassert>
#include <cstdint>

#include <string.h>

class OggStreamState {
	ogg_stream_state state;

public:
	explicit OggStreamState(int serialno) noexcept {
		ogg_stream_init(&state, serialno);
	}

	/**
	 * Initialize a decoding #ogg_stream_state with the first
	 * page.
	 */
	explicit OggStreamState(ogg_page &page) noexcept {
		ogg_stream_init(&state, ogg_page_serialno(&page));
		PageIn(page);
	}

	~OggStreamState() noexcept {
		ogg_stream_clear(&state);
	}

	operator ogg_stream_state &() noexcept {
		return state;
	}

	void Reinitialize(int serialno) noexcept {
		ogg_stream_reset_serialno(&state, serialno);
	}

	long GetSerialNo() const noexcept {
		return state.serialno;
	}

	void Reset() noexcept {
		ogg_stream_reset(&state);
	}

	/* encoding */

	void PacketIn(const ogg_packet &packet) noexcept {
		ogg_stream_packetin(&state,
				    const_cast<ogg_packet *>(&packet));
	}

	bool PageOut(ogg_page &page) noexcept {
		return ogg_stream_pageout(&state, &page) != 0;
	}

	bool Flush(ogg_page &page) noexcept {
		return ogg_stream_flush(&state, &page) != 0;
	}

	/* decoding */

	bool PageIn(ogg_page &page) noexcept {
		return ogg_stream_pagein(&state, &page) == 0;
	}

	int PacketOut(ogg_packet &packet) noexcept {
		return ogg_stream_packetout(&state, &packet);
	}
};

#endif
