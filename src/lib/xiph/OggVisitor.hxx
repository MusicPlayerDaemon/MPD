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

#ifndef MPD_OGG_VISITOR_HXX
#define MPD_OGG_VISITOR_HXX

#include "OggSyncState.hxx"
#include "OggStreamState.hxx"

#include <ogg/ogg.h>

class Reader;

/**
 * Abstract class which iterates over Ogg packets in a #Reader.
 * Subclass it and implement the virtual methods.
 */
class OggVisitor {
	OggSyncState sync;
	OggStreamState stream;

	bool has_stream = false;

	/**
	 * This is true after seeking; its one-time effect is to
	 * ignore the BOS packet, just in case we have been seeking to
	 * the beginning of the file, because that would disrupt
	 * playback.
	 */
	bool post_seek = false;

public:
	explicit OggVisitor(Reader &reader)
		:sync(reader), stream(0) {}

	long GetSerialNo() const {
		return stream.GetSerialNo();
	}

	uint64_t GetStartOffset() const noexcept {
		return sync.GetStartOffset();
	}

	void Visit();

	/**
	 * Call this method after seeking the #Reader.
	 *
	 * @param offset the current #Reader offset
	 */
	void PostSeek(uint64_t offset);

	/**
	 * Skip packets (#ogg_packet) from the #OggStreamState until a
	 * packet with a valid granulepos is found or until the stream
	 * has run dry.
	 *
	 * Since this will discard pending packets and will disturb
	 * this object, this should only be used while seeking.
	 *
	 * This method must not be called from within one of the
	 * virtual methods.
	 *
	 * @return the granulepos or -1 if no valid granulepos was
	 * found
	 */
	ogg_int64_t ReadGranulepos() noexcept;

private:
	void EndStream();
	bool ReadNextPage();
	void HandlePacket(const ogg_packet &packet);
	void HandlePackets();

protected:
	/**
	 * Called when the "beginning of stream" packet has been seen.
	 *
	 * @param packet the "beginning of stream" packet
	 */
	virtual void OnOggBeginning(const ogg_packet &packet) = 0;

	/**
	 * Called for each follow-up packet.
	 */
	virtual void OnOggPacket(const ogg_packet &packet) = 0;

	/**
	 * Called after the "end of stream" packet has been processed.
	 */
	virtual void OnOggEnd() = 0;
};

#endif
