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

		/* passing synced=false because we're inside an
		   OggVisitor callback, and our InputStream may be in
		   the middle of an Ogg packet */
		result = OggSeekFindEOS(sync2, stream2, packet,
					input_stream, false);
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

inline void
OggDecoder::SeekByte(offset_type offset)
{
	input_stream.LockSeek(offset);
	PostSeek(offset);
}

void
OggDecoder::SeekGranulePos(ogg_int64_t where_granulepos)
{
	assert(IsSeekable());

	/* binary search: interpolate the file offset where we expect
	   to find the given granule position, and repeat until we're
	   close enough */

	static const ogg_int64_t MARGIN_BEFORE = 44100 / 3;
	static const ogg_int64_t MARGIN_AFTER = 44100 / 10;

	offset_type min_offset = 0, max_offset = input_stream.GetSize();
	ogg_int64_t min_granule = 0, max_granule = end_granulepos;

	while (true) {
		const offset_type delta_offset = max_offset - min_offset;
		const ogg_int64_t delta_granule = max_granule - min_granule;
		const ogg_int64_t relative_granule = where_granulepos - min_granule;

		const offset_type offset = min_offset + relative_granule * delta_offset
			/ delta_granule;

		SeekByte(offset);

		const auto new_granule = ReadGranulepos();
		if (new_granule < 0)
			/* no granulepos here, which shouldn't happen
			   - we can't improve, so stop */
			return;

		if (new_granule > where_granulepos + MARGIN_AFTER) {
			if (new_granule > max_granule)
				/* something went wrong */
				return;

			if (max_granule == new_granule)
				/* break out of the infinite loop, we
				   can't get any closer */
				break;

			/* reduce the max bounds and interpolate again */
			max_granule = new_granule;
			max_offset = GetStartOffset();
		} else if (new_granule + MARGIN_BEFORE < where_granulepos) {
			if (new_granule < min_granule)
				/* something went wrong */
				return;

			if (min_granule == new_granule)
				/* break out of the infinite loop, we
				   can't get any closer */
				break;

			/* increase the min bounds and interpolate
			   again */
			min_granule = new_granule;
			min_offset = GetStartOffset();
		} else {
			break;
		}
	}

	/* go back to the last page start so OggVisitor can start
	   visiting from here (we have consumed a few pages
	   already) */
	SeekByte(GetStartOffset());
}
