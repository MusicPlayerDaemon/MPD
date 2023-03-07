// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Registry.hxx"
#include "FilterPlugin.hxx"
#include "plugins/NullFilterPlugin.hxx"
#include "plugins/RouteFilterPlugin.hxx"
#include "plugins/NormalizeFilterPlugin.hxx"
#include "plugins/FfmpegFilterPlugin.hxx"
#include "plugins/HdcdFilterPlugin.hxx"
#include "config.h"

#include <string.h>

static constinit const FilterPlugin *const filter_plugins[] = {
	&null_filter_plugin,
	&route_filter_plugin,
	&normalize_filter_plugin,
#ifdef HAVE_LIBAVFILTER
	&ffmpeg_filter_plugin,
	&hdcd_filter_plugin,
#endif
	nullptr,
};

const FilterPlugin *
filter_plugin_by_name(const char *name) noexcept
{
	for (unsigned i = 0; filter_plugins[i] != nullptr; ++i)
		if (strcmp(filter_plugins[i]->name, name) == 0)
			return filter_plugins[i];

	return nullptr;
}
