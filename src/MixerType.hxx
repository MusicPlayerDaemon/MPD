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

#ifndef MPD_MIXER_TYPE_HXX
#define MPD_MIXER_TYPE_HXX

enum mixer_type {
	/** parser error */
	MIXER_TYPE_UNKNOWN,

	/** mixer disabled */
	MIXER_TYPE_NONE,

	/** software mixer with pcm_volume() */
	MIXER_TYPE_SOFTWARE,

	/** hardware mixer (output's plugin) */
	MIXER_TYPE_HARDWARE,
};

/**
 * Parses a "mixer_type" setting from the configuration file.
 *
 * @param input the configured string value; must not be NULL
 * @return a #mixer_type value; MIXER_TYPE_UNKNOWN means #input could
 * not be parsed
 */
enum mixer_type
mixer_type_parse(const char *input);

#endif
