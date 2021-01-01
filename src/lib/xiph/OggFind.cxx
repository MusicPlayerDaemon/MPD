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

#include "OggFind.hxx"
#include "lib/xiph/OggSyncState.hxx"
#include "input/InputStream.hxx"

bool
OggFindEOS(OggSyncState &oy, ogg_stream_state &os, ogg_packet &packet)
{
	while (true) {
		int r = ogg_stream_packetout(&os, &packet);
		if (r == 0) {
			if (!oy.ExpectPageIn(os))
				return false;

			continue;
		} else if (r > 0 && packet.e_o_s)
			return true;
	}
}

bool
OggSeekPageAtOffset(OggSyncState &oy, ogg_stream_state &os, InputStream &is,
		    offset_type offset)
{
	oy.Reset();

	/* reset the stream to clear any previous partial packet
	   data */
	ogg_stream_reset(&os);

	try {
		is.LockSeek(offset);
	} catch (...) {
		return false;
	}

	return oy.ExpectPageSeekIn(os);
}

bool
OggSeekFindEOS(OggSyncState &oy, ogg_stream_state &os, ogg_packet &packet,
	       InputStream &is, bool synced)
{
	if (!is.KnownSize())
		return false;

	if (is.GetRest() < 65536)
		return (synced || oy.ExpectPageSeekIn(os)) &&
			OggFindEOS(oy, os, packet);

	if (!is.CheapSeeking())
		return false;

	return OggSeekPageAtOffset(oy, os, is, is.GetSize() - 65536) &&
		OggFindEOS(oy, os, packet);
}
