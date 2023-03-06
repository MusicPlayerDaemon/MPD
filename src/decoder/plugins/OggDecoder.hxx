// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_OGG_DECODER_HXX
#define MPD_OGG_DECODER_HXX

#include "lib/xiph/OggVisitor.hxx"
#include "decoder/Reader.hxx"
#include "input/Offset.hxx"

class OggDecoder : public OggVisitor {
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
