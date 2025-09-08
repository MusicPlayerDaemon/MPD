// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_OGG_DECODER_HXX
#define MPD_OGG_DECODER_HXX

#include "lib/xiph/OggVisitor.hxx"
#include "decoder/Reader.hxx"
#include "input/Offset.hxx"

class OggDecoder : public OggVisitor {
	/**
	 * The file offset of the first audio packet (starts at
	 * granulepos 0).  This is used by SeekGranulePos() to
	 * interpolate the seek offset between this offset and
	 * end-of-file, possibly skipping (large) tags preceding the
	 * first audio packet.
	 */
	offset_type first_offset = 0;

	/**
	 * The granulepos at the end of the last packet.  This is used
	 * to calculate the song duration and to calculate seek file
	 * offsets.
	 *
	 * This field is uninitialized until UpdateEndGranulePos() is
	 * called.
	 */
	ogg_int64_t end_granulepos;

protected:
	DecoderClient &client;
	InputStream &input_stream;

public:
	explicit OggDecoder(DecoderReader &reader)
		:OggVisitor(reader),
		 client(reader.GetClient()),
		 input_stream(reader.GetInputStream()) {}

private:
	/**
	 * Load the end-of-stream packet and restore the previous file
	 * position.
	 */
	bool LoadEndPacket(ogg_packet &packet) const;
	ogg_int64_t LoadEndGranulePos() const;

protected:
	bool HasFirstOffset() const noexcept {
		return first_offset > 0;
	}

	void SetFirstOffset(offset_type _first_offset) noexcept {
		first_offset = _first_offset;
	}

	/**
	 * If currently unset, set the #first_offset field to the
	 * start of the most recent Ogg page.  Decoder implementations
	 * should call this when they see the first page/packet
	 * containing audio data.
	 */
	void AutoSetFirstOffset() noexcept {
		if (!HasFirstOffset())
			SetFirstOffset(GetStartOffset());
	}

	ogg_int64_t UpdateEndGranulePos() {
		return end_granulepos = LoadEndGranulePos();
	}

	bool IsSeekable() const {
		return end_granulepos > 0;
	}

	/**
	 * Seek the #InputStream and update the #OggVisitor.
	 *
	 * Throws on error.
	 */
	void SeekByte(offset_type offset);

	void SeekGranulePos(ogg_int64_t where_granulepos);
};

#endif
