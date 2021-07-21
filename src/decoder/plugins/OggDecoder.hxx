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
