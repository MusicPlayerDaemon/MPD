/*
 * Copyright (C) 2003-2010 The Music Player Daemon Project
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
#include "tag_rva2.h"
#include "replay_gain_info.h"

#include <glib.h>
#include <id3tag.h>

enum rva2_channel {
	CHANNEL_OTHER = 0x00,
	CHANNEL_MASTER_VOLUME = 0x01,
	CHANNEL_FRONT_RIGHT = 0x02,
	CHANNEL_FRONT_LEFT = 0x03,
	CHANNEL_BACK_RIGHT = 0x04,
	CHANNEL_BACK_LEFT = 0x05,
	CHANNEL_FRONT_CENTRE = 0x06,
	CHANNEL_BACK_CENTRE = 0x07,
	CHANNEL_SUBWOOFER = 0x08
};

bool
tag_rva2_parse(struct id3_tag *tag, struct replay_gain_info *replay_gain_info)
{
	struct id3_frame const * frame;

	id3_latin1_t const *id;
	id3_byte_t const *data;
	id3_length_t length;

	/* relative volume adjustment information */

	frame = id3_tag_findframe(tag, "RVA2", 0);
	if (frame == NULL)
		return false;

	id   = id3_field_getlatin1(id3_frame_field(frame, 0));
	data = id3_field_getbinarydata(id3_frame_field(frame, 1),
					&length);

	if (id == NULL || data == NULL)
		return false;

	/*
	 * "The 'identification' string is used to identify the
	 * situation and/or device where this adjustment should apply.
	 * The following is then repeated for every channel
	 *
	 *   Type of channel         $xx
	 *   Volume adjustment       $xx xx
	 *   Bits representing peak  $xx
	 *   Peak volume             $xx (xx ...)"
	 */

	while (length >= 4) {
		unsigned int peak_bytes;

		peak_bytes = (data[3] + 7) / 8;
		if (4 + peak_bytes > length)
			break;

		if (data[0] == CHANNEL_MASTER_VOLUME) {
			signed int voladj_fixed;
			double voladj_float;

			/*
			 * "The volume adjustment is encoded as a fixed
			 * point decibel value, 16 bit signed integer
			 * representing (adjustment*512), giving +/- 64
			 * dB with a precision of 0.001953125 dB."
			 */

			voladj_fixed  = (data[1] << 8) | (data[2] << 0);
			voladj_fixed |= -(voladj_fixed & 0x8000);

			voladj_float  = (double) voladj_fixed / 512;

			replay_gain_info->tuples[REPLAY_GAIN_TRACK].gain = voladj_float;
			replay_gain_info->tuples[REPLAY_GAIN_ALBUM].gain = voladj_float;

			g_debug("parseRVA2: Relative Volume "
				"%+.1f dB adjustment (%s)\n",
				voladj_float, id);

			return true;
		}

		data   += 4 + peak_bytes;
		length -= 4 + peak_bytes;
	}

	return false;
}
