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

#include "Init.hxx"
#include "Registry.hxx"
#include "InputPlugin.hxx"
#include "config/Data.hxx"
#include "config/Option.hxx"
#include "config/Block.hxx"
#include "Log.hxx"
#include "PluginUnavailable.hxx"
#include "util/Domain.hxx"
#include "util/RuntimeError.hxx"

#include <cassert>
#include <stdexcept>

#include "io/uring/Features.h"
#ifdef HAVE_URING
#include "plugins/UringInputPlugin.hxx"
#endif

static constexpr Domain input_domain("input");

void
input_stream_global_init(const ConfigData &config, EventLoop &event_loop)
{
#ifdef HAVE_URING
	InitUringInputPlugin(event_loop);
#endif

	const ConfigBlock empty;

	for (unsigned i = 0; input_plugins[i] != nullptr; ++i) {
		const InputPlugin *plugin = input_plugins[i];

		assert(plugin->name != nullptr);
		assert(*plugin->name != 0);
		assert(plugin->open != nullptr);

		const auto *block =
			config.FindBlock(ConfigBlockOption::INPUT, "plugin",
					 plugin->name);
		if (block == nullptr) {
			block = &empty;
		} else if (!block->GetBlockValue("enabled", true))
			/* the plugin is disabled in mpd.conf */
			continue;

		block->SetUsed();

		try {
			if (plugin->init != nullptr)
				plugin->init(event_loop, *block);
			input_plugins_enabled[i] = true;
		} catch (const PluginUnconfigured &e) {
			FmtDebug(input_domain,
				 "Input plugin '{}' is not configured: {}",
				 plugin->name, e.what());
			continue;
		} catch (const PluginUnavailable &e) {
			FmtDebug(input_domain,
				 "Input plugin '{}' is unavailable: {}",
				 plugin->name, e.what());
			continue;
		} catch (...) {
			std::throw_with_nested(FormatRuntimeError("Failed to initialize input plugin '%s'",
								  plugin->name));
		}
	}
}

void
input_stream_global_finish() noexcept
{
	input_plugins_for_each_enabled(plugin)
		if (plugin->finish != nullptr)
			plugin->finish();
}
