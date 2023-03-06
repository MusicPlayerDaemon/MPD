// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "StateFileConfig.hxx"
#include "config/Data.hxx"

#ifdef ANDROID
#include "fs/glue/StandardDirectory.hxx"
#endif

StateFileConfig::StateFileConfig(const ConfigData &config)
	:path(config.GetPath(ConfigOption::STATE_FILE)),
	 interval(config.GetUnsigned(ConfigOption::STATE_FILE_INTERVAL,
				     DEFAULT_INTERVAL)),
	 restore_paused(config.GetBool(ConfigOption::RESTORE_PAUSED, false))
{
#ifdef ANDROID
	if (path.IsNull()) {
		const auto cache_dir = GetAppCacheDir();
		if (cache_dir.IsNull())
			return;

		path = cache_dir / Path::FromFS("state");
	}
#endif
}
