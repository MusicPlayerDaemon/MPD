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

#include "StateFileConfig.hxx"
#include "config/Data.hxx"

#ifdef ANDROID
#include "fs/StandardDirectory.hxx"
#endif

StateFileConfig::StateFileConfig(const ConfigData &config)
	:path(config.GetPath(ConfigOption::STATE_FILE)),
	 interval(config.GetUnsigned(ConfigOption::STATE_FILE_INTERVAL,
				     DEFAULT_INTERVAL)),
	 restore_paused(config.GetBool(ConfigOption::RESTORE_PAUSED, false))
{
#ifdef ANDROID
	if (path.IsNull()) {
		const auto cache_dir = GetUserCacheDir();
		if (cache_dir.IsNull())
			return;

		path = cache_dir / Path::FromFS("state");
	}
#endif
}
