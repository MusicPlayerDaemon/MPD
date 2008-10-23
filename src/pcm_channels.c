/* the Music Player Daemon (MPD)
 * Copyright (C) 2008 Max Kellermann <max@duempel.org>
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "pcm_channels.h"
#include "utils.h"
#include "log.h"

#include <assert.h>

static void
pcm_convert_channels_16_1_to_2(int16_t *dest, const int16_t *src,
			       unsigned num_frames)
{
	while (num_frames-- > 0) {
		int16_t value = *src++;

		*dest++ = value;
		*dest++ = value;
	}
}

static void
pcm_convert_channels_16_2_to_1(int16_t *dest, const int16_t *src,
			       unsigned num_frames)
{
	while (num_frames-- > 0) {
		int32_t a = *src++, b = *src++;

		*dest++ = (a + b) / 2;
	}
}

static void
pcm_convert_channels_16_n_to_2(int16_t *dest,
			       unsigned src_channels, const int16_t *src,
			       unsigned num_frames)
{
	unsigned c;

	assert(src_channels > 0);

	while (num_frames-- > 0) {
		int32_t sum = 0;
		int16_t value;

		for (c = 0; c < src_channels; ++c)
			sum += *src++;
		value = sum / (int)src_channels;

		/* XXX this is actually only mono ... */
		*dest++ = value;
		*dest++ = value;
	}
}

const int16_t *
pcm_convert_channels_16(int8_t dest_channels,
			int8_t src_channels, const int16_t *src,
			size_t src_size, size_t *dest_size_r)
{
	static int16_t *buf;
	static size_t len;
	unsigned num_frames = src_size / src_channels / sizeof(*src);
	unsigned dest_size = num_frames * dest_channels * sizeof(*src);

	if (dest_size > len) {
		len = dest_size;
		buf = xrealloc(buf, len);
	}

	*dest_size_r = dest_size;

	if (src_channels == 1 && dest_channels == 2)
		pcm_convert_channels_16_1_to_2(buf, src, num_frames);
	else if (src_channels == 2 && dest_channels == 1)
		pcm_convert_channels_16_2_to_1(buf, src, num_frames);
	else if (dest_channels == 2)
		pcm_convert_channels_16_n_to_2(buf, src_channels, src,
					       num_frames);
	else {
		ERROR("conversion %u->%u channels is not supported\n",
		      src_channels, dest_channels);
		return NULL;
	}

	return buf;
}

static void
pcm_convert_channels_24_1_to_2(int32_t *dest, const int32_t *src,
			       unsigned num_frames)
{
	while (num_frames-- > 0) {
		int32_t value = *src++;

		*dest++ = value;
		*dest++ = value;
	}
}

static void
pcm_convert_channels_24_2_to_1(int32_t *dest, const int32_t *src,
			       unsigned num_frames)
{
	while (num_frames-- > 0) {
		int32_t a = *src++, b = *src++;

		*dest++ = (a + b) / 2;
	}
}

static void
pcm_convert_channels_24_n_to_2(int32_t *dest,
			       unsigned src_channels, const int32_t *src,
			       unsigned num_frames)
{
	unsigned c;

	assert(src_channels > 0);

	while (num_frames-- > 0) {
		int32_t sum = 0;
		int32_t value;

		for (c = 0; c < src_channels; ++c)
			sum += *src++;
		value = sum / (int)src_channels;

		/* XXX this is actually only mono ... */
		*dest++ = value;
		*dest++ = value;
	}
}

const int32_t *
pcm_convert_channels_24(int8_t dest_channels,
			int8_t src_channels, const int32_t *src,
			size_t src_size, size_t *dest_size_r)
{
	static int32_t *buf;
	static size_t len;
	unsigned num_frames = src_size / src_channels / sizeof(*src);
	unsigned dest_size = num_frames * dest_channels * sizeof(*src);

	if (dest_size > len) {
		len = dest_size;
		buf = xrealloc(buf, len);
	}

	*dest_size_r = dest_size;

	if (src_channels == 1 && dest_channels == 2)
		pcm_convert_channels_24_1_to_2(buf, src, num_frames);
	else if (src_channels == 2 && dest_channels == 1)
		pcm_convert_channels_24_2_to_1(buf, src, num_frames);
	else if (dest_channels == 2)
		pcm_convert_channels_24_n_to_2(buf, src_channels, src,
					       num_frames);
	else {
		ERROR("conversion %u->%u channels is not supported\n",
		      src_channels, dest_channels);
		return NULL;
	}

	return buf;
}
