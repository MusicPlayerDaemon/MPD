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

#include "ArchiveList.hxx"
#include "ArchivePlugin.hxx"
#include "archive/Features.h"
#include "config/Data.hxx"
#include "config/Block.hxx"
#include "util/StringUtil.hxx"
#include "plugins/Bzip2ArchivePlugin.hxx"
#include "plugins/Iso9660ArchivePlugin.hxx"
#include "plugins/ZzipArchivePlugin.hxx"

#include <cassert>

#include <string.h>

constexpr const ArchivePlugin *archive_plugins[] = {
#ifdef ENABLE_BZ2
	&bz2_archive_plugin,
#endif
#ifdef ENABLE_ZZIP
	&zzip_archive_plugin,
#endif
#ifdef ENABLE_ISO9660
	&iso9660_archive_plugin,
#endif
	nullptr
};

static constexpr std::size_t n_archive_plugins = std::size(archive_plugins) - 1;

/** which plugins have been initialized successfully? */
/* the std::max() is just here to avoid a zero-sized array, which is
   forbidden in C++ */
static bool archive_plugins_enabled[std::max(n_archive_plugins, std::size_t(1))];

#define archive_plugins_for_each_enabled(plugin) \
	archive_plugins_for_each(plugin) \
		if (archive_plugins_enabled[archive_plugin_iterator - archive_plugins])

const ArchivePlugin *
archive_plugin_from_suffix(std::string_view suffix) noexcept
{
	archive_plugins_for_each_enabled(plugin)
		if (plugin->suffixes != nullptr &&
		    StringArrayContainsCase(plugin->suffixes, suffix))
			return plugin;

	return nullptr;
}

const ArchivePlugin *
archive_plugin_from_name(const char *name) noexcept
{
	archive_plugins_for_each_enabled(plugin)
		if (strcmp(plugin->name, name) == 0)
			return plugin;

	return nullptr;
}

void
archive_plugin_init_all(const ConfigData &config)
{
	ConfigBlock empty;

	for (unsigned i = 0; archive_plugins[i] != nullptr; ++i) {
		const auto &plugin = *archive_plugins[i];

		const auto *param =
			config.FindBlock(ConfigBlockOption::ARCHIVE_PLUGIN,
					 "name", plugin.name);
		if (param != nullptr && !param->GetBlockValue("enabled", true))
			/* the plugin is disabled in mpd.conf */
			continue;

		if (plugin.init == nullptr || plugin.init())
			archive_plugins_enabled[i] = true;
	}
}

void
archive_plugin_deinit_all() noexcept
{
	unsigned i = 0;
	archive_plugins_for_each_enabled(plugin)
		if (archive_plugins_enabled[i++] && plugin->finish != nullptr)
			plugin->finish();
}

