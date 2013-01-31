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

#ifndef MPD_TEST_PCM_ALL_HXX
#define MPD_TEST_PCM_ALL_HXX

void
test_pcm_dither_24();

void
test_pcm_dither_32();

void
test_pcm_pack_24();

void
test_pcm_unpack_24();

void
test_pcm_channels_16();

void
test_pcm_channels_32();

void
test_pcm_volume_8();

void
test_pcm_volume_16();

void
test_pcm_volume_24();

void
test_pcm_volume_32();

void
test_pcm_volume_float();

void
test_pcm_format_8_to_16();

void
test_pcm_format_16_to_24();

void
test_pcm_format_16_to_32();

void
test_pcm_format_float();

#endif
