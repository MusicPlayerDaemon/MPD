/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "TagRva2.hxx"
#include "ReplayGainInfo.hxx"

#include <id3tag.h>

#include <stdint.h>
#include <string.h>

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

struct rva2_data {
	uint8_t type;
	uint8_t volume_adjustment[2];
	uint8_t peak_bits;
};

static inline id3_length_t
rva2_peak_bytes(const struct rva2_data *data)
{
	return (data->peak_bits + 7) / 8;
}

static inline int
rva2_fixed_volume_adjustment(const struct rva2_data *data)
{
	signed int voladj_fixed;
	voladj_fixed = (data->volume_adjustment[0] << 8) |
		data->volume_adjustment[1];
	voladj_fixed |= -(voladj_fixed & 0x8000);
	return voladj_fixed;
}

static inline float
rva2_float_volume_adjustment(const struct rva2_data *data)
{
	/*
	 * "The volume adjustment is encoded as a fixed point decibel
	 * value, 16 bit signed integer representing (adjustment*512),
	 * giving +/- 64 dB with a precision of 0.001953125 dB."
	 */

	return (float)rva2_fixed_volume_adjustment(data) / (float)512;
}

static inline bool
rva2_apply_data(ReplayGainInfo &rgi,
		const struct rva2_data *data, const id3_latin1_t *id)
{
	if (data->type != CHANNEL_MASTER_VOLUME)
		return false;

	float volume_adjustment = rva2_float_volume_adjustment(data);

	if (strcmp((const char *)id, "album") == 0)  {
		rgi.tuples[REPLAY_GAIN_ALBUM].gain = volume_adjustment;
	} else if (strcmp((const char *)id, "track") == 0) {
		rgi.tuples[REPLAY_GAIN_TRACK].gain = volume_adjustment;
	} else {
		rgi.tuples[REPLAY_GAIN_ALBUM].gain = volume_adjustment;
		rgi.tuples[REPLAY_GAIN_TRACK].gain = volume_adjustment;
	}

	return true;
}

static bool
rva2_apply_frame(ReplayGainInfo &replay_gain_info,
		 const struct id3_frame *frame)
{
	const id3_latin1_t *id = id3_field_getlatin1(id3_frame_field(frame, 0));
	id3_length_t length;
	const id3_byte_t *data =
		id3_field_getbinarydata(id3_frame_field(frame, 1), &length);

	if (id == nullptr || data == nullptr)
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
		const struct rva2_data *d = (const struct rva2_data *)data;
		unsigned int peak_bytes = rva2_peak_bytes(d);
		if (4 + peak_bytes > length)
			break;

		if (rva2_apply_data(replay_gain_info, d, id))
			return true;

		data   += 4 + peak_bytes;
		length -= 4 + peak_bytes;
	}

	return false;
}

bool
tag_rva2_parse(struct id3_tag *tag, ReplayGainInfo &replay_gain_info)
{
	bool found = false;

	/* Loop through all RVA2 frames as some programs (e.g. mp3gain) store
	   track and album gain in separate tags */
	const struct id3_frame *frame;
	for (unsigned i = 0;
	     (frame = id3_tag_findframe(tag, "RVA2", i)) != nullptr;
	     ++i)
		if (rva2_apply_frame(replay_gain_info, frame))
			found = true;

	return found;
}
