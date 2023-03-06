// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
