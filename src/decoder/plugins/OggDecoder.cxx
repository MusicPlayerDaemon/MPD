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

#include "OggDecoder.hxx"
#include "lib/xiph/OggFind.hxx"
#include "input/InputStream.hxx"

/**
 * Load the end-of-stream packet and restore the previous file
 * position.
 */
bool
OggDecoder::LoadEndPacket(ogg_packet &packet) const
{
	if (!input_stream.CheapSeeking())
		/* we do this for local files only, because seeking
		   around remote files is expensive and not worth the
		   trouble */
		return false;

	const auto old_offset = input_stream.GetOffset();

	/* create temporary Ogg objects for seeking and parsing the
	   EOS packet */

	bool result;

	{
		DecoderReader reader(client, input_stream);
		OggSyncState sync2(reader);
		OggStreamState stream2(GetSerialNo());
		result = OggSeekFindEOS(sync2, stream2, packet,
					input_stream);
	}

	/* restore the previous file position */
	try {
		input_stream.LockSeek(old_offset);
	} catch (...) {
	}

	return result;
}

ogg_int64_t
OggDecoder::LoadEndGranulePos() const
{
	ogg_packet packet;
	if (!LoadEndPacket(packet))
		return -1;

	return packet.granulepos;
}

void
OggDecoder::SeekGranulePos(ogg_int64_t where_granulepos)
{
	assert(IsSeekable());

	/* interpolate the file offset where we expect to find the
	   given granule position */
	/* TODO: implement binary search */
	offset_type offset(where_granulepos * input_stream.GetSize()
			   / end_granulepos);

	input_stream.LockSeek(offset);
	PostSeek();
}

