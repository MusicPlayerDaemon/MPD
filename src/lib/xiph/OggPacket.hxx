// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_OGG_PACKET_HXX
#define MPD_OGG_PACKET_HXX

#include <ogg/ogg.h>

class OggSyncState;
class OggStreamState;

/**
 * Read the next packet.  If necessary, feed more data into
 * #OggSyncState and feed more pages into #OggStreamState.
 */
bool
OggReadPacket(OggSyncState &sync, OggStreamState &stream, ogg_packet &packet);

#endif
