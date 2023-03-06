// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_OGG_FIND_HXX
#define MPD_OGG_FIND_HXX

#include "input/Offset.hxx"

#include <ogg/ogg.h>

class OggSyncState;
class InputStream;

/**
 * Skip all pages/packets until an end-of-stream (EOS) packet for the
 * specified stream is found.
 *
 * @return true if the EOS packet was found
 */
bool
OggFindEOS(OggSyncState &oy, ogg_stream_state &os, ogg_packet &packet);

/**
 * Seek the #InputStream and find the next Ogg page.
 */
bool
OggSeekPageAtOffset(OggSyncState &oy, ogg_stream_state &os, InputStream &is,
		    offset_type offset);

/**
 * Try to find the end-of-stream (EOS) packet.  Seek to the end of the
 * file if necessary.
 *
 * @param synced is the #OggSyncState currently synced?  If not, then
 * we need to use ogg_sync_pageseek() instead of ogg_sync_pageout(),
 * which is more expensive
 * @return true if the EOS packet was found
 */
bool
OggSeekFindEOS(OggSyncState &oy, ogg_stream_state &os, ogg_packet &packet,
	       InputStream &is, bool synced=true);

#endif
