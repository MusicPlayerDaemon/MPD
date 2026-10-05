// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "OggPacket.hxx"
#include "OggSyncState.hxx"
#include "OggStreamState.hxx"

bool
OggReadPacket(OggSyncState &sync, OggStreamState &stream, ogg_packet &packet)
{
	while (true) {
		const int result = stream.PacketOut(packet);
		if (result > 0)
			return true;

		/* a negative value indicates a gap in the stream and
		   doesn't fill the "packet"; try again */
		if (result == 0 && !sync.ExpectPageIn(stream))
			return false;
	}
}
