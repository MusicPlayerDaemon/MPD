/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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

#include "config.h"
#include "OggFind.hxx"
#include "OggSyncState.hxx"

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
