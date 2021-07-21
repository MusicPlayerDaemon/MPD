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

#include "Registry.hxx"
#include "FilterPlugin.hxx"
#include "plugins/NullFilterPlugin.hxx"
#include "plugins/RouteFilterPlugin.hxx"
#include "plugins/NormalizeFilterPlugin.hxx"
#include "plugins/FfmpegFilterPlugin.hxx"
#include "plugins/HdcdFilterPlugin.hxx"
#include "config.h"

#include <string.h>

static constexpr const FilterPlugin *filter_plugins[] = {
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
