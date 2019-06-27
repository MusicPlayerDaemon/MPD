/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include <stddef.h>

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

	void Visit();

	/**
	 * Call this method after seeking the #Reader.
	 */
	void PostSeek();

private:
	void EndStream();
	bool ReadNextPage();
	void HandlePacket(const ogg_packet &packet);
	void HandlePackets();

protected:
	virtual void OnOggBeginning(const ogg_packet &packet) = 0;
	virtual void OnOggPacket(const ogg_packet &packet) = 0;
	virtual void OnOggEnd() = 0;
};

#endif
