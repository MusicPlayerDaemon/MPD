// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_ARCHIVE_LIST_HXX
#define MPD_ARCHIVE_LIST_HXX

#include <string_view>

struct ConfigData;
struct ArchivePlugin;

extern const ArchivePlugin *const archive_plugins[];

#define archive_plugins_for_each(plugin) \
	for (const ArchivePlugin *plugin, \
		*const*archive_plugin_iterator = &archive_plugins[0]; \
		(plugin = *archive_plugin_iterator) != nullptr; \
		++archive_plugin_iterator)

/* interface for using plugins */

const ArchivePlugin *
archive_plugin_from_suffix(std::string_view suffix) noexcept;

const ArchivePlugin *
archive_plugin_from_name(const char *name) noexcept;

/* this is where we "load" all the "plugins" ;-) */
void
archive_plugin_init_all(const ConfigData &config);

/* this is where we "unload" all the "plugins" */
void
archive_plugin_deinit_all() noexcept;

class ScopeArchivePluginsInit {
public:
	explicit ScopeArchivePluginsInit(const ConfigData &config) {
		archive_plugin_init_all(config);
	}

	~ScopeArchivePluginsInit() noexcept {
		archive_plugin_deinit_all();
	}
};

#endif
