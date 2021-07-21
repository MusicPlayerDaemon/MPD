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

#ifndef MPD_STATE_FILE_CONFIG_HXX
#define MPD_STATE_FILE_CONFIG_HXX

#include "fs/AllocatedPath.hxx"
#include "event/Chrono.hxx"

struct ConfigData;

struct StateFileConfig {
	static constexpr Event::Duration DEFAULT_INTERVAL = std::chrono::minutes(2);

	AllocatedPath path;

	Event::Duration interval;

	bool restore_paused;

	explicit StateFileConfig(const ConfigData &config);

	bool IsEnabled() const noexcept {
		return !path.IsNull();
	}
};

#endif
