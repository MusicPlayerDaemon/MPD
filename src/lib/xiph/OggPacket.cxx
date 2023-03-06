// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "OggPacket.hxx"
#include "OggSyncState.hxx"
#include "OggStreamState.hxx"

bool
OggReadPacket(OggSyncState &sync, OggStreamState &stream, ogg_packet &packet)
{
	while (true) {
		if (stream.PacketOut(packet))
			return true;

		if (!sync.ExpectPageIn(stream))
			return false;
	}
}
