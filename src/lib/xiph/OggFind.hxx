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
