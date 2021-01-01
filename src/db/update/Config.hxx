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

#ifndef MPD_UPDATE_CONFIG_HXX
#define MPD_UPDATE_CONFIG_HXX

struct ConfigData;

struct UpdateConfig {
#ifndef _WIN32
	static constexpr bool DEFAULT_FOLLOW_INSIDE_SYMLINKS = true;
	static constexpr bool DEFAULT_FOLLOW_OUTSIDE_SYMLINKS = true;

	bool follow_inside_symlinks = DEFAULT_FOLLOW_INSIDE_SYMLINKS;
	bool follow_outside_symlinks = DEFAULT_FOLLOW_OUTSIDE_SYMLINKS;
#endif

	explicit UpdateConfig(const ConfigData &config);
};

#endif
