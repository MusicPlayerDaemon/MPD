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

#ifndef MPD_PCM_DITHER_HXX
#define MPD_PCM_DITHER_HXX

#include <stdint.h>

class PcmDither {
	int32_t error[3];
	int32_t random;

public:
	constexpr PcmDither()
		:error{0, 0, 0}, random(0) {}

	void Dither24To16(int16_t *dest, const int32_t *src,
			  const int32_t *src_end);

	void Dither32To16(int16_t *dest, const int32_t *src,
			  const int32_t *src_end);

private:
	int16_t Dither24To16(int_fast32_t sample);
	int16_t Dither32To16(int_fast32_t sample);
};

#endif
