// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
