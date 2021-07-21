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

#ifndef MPD_MIXER_TYPE_HXX
#define MPD_MIXER_TYPE_HXX

enum class MixerType {
	/** mixer disabled */
	NONE,

	/** "null" mixer (virtual fake) */
	NULL_,

	/** software mixer with pcm_volume() */
	SOFTWARE,

	/** hardware mixer (output's plugin) */
	HARDWARE,
};

/**
 * Parses a #MixerType setting from the configuration file.
 *
 * Throws if the string could not be parsed.
 *
 * @param input the configured string value
 * @return a #MixerType value
 */
MixerType
mixer_type_parse(const char *input);

#endif
