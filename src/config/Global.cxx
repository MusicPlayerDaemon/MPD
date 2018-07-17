/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

#include "config.h"
#include "Global.hxx"
#include "Data.hxx"
#include "Param.hxx"
#include "Block.hxx"
#include "File.hxx"
#include "Path.hxx"
#include "Domain.hxx"
#include "fs/Path.hxx"
#include "fs/AllocatedPath.hxx"
#include "util/RuntimeError.hxx"
#include "Log.hxx"

static ConfigData config_data;

void config_global_finish(void)
{
	config_data.Clear();
}

void config_global_init(void)
{
}

const ConfigData &
GetGlobalConfig() noexcept
{
	return config_data;
}

void
ReadConfigFile(Path path)
{
	return ReadConfigFile(config_data, path);
}

static void
Check(const ConfigBlock &block)
{
	if (!block.used)
		/* this whole block was not queried at all -
		   the feature might be disabled at compile time?
		   Silently ignore it here. */
		return;

	for (const auto &i : block.block_params) {
		if (!i.used)
			FormatWarning(config_domain,
				      "option '%s' on line %i was not recognized",
				      i.name.c_str(), i.line);
	}
}

void config_global_check(void)
{
	for (auto i : config_data.blocks)
		for (const auto *p = i; p != nullptr; p = p->next)
			Check(*p);
}

const ConfigParam *
config_get_param(ConfigOption option) noexcept
{
	return config_data.GetParam(option);
}

const ConfigBlock *
config_get_block(ConfigBlockOption option) noexcept
{
	return config_data.GetBlock(option);
}

const char *
config_get_string(ConfigOption option, const char *default_value) noexcept
{
	return config_data.GetString(option, default_value);
}

AllocatedPath
config_get_path(ConfigOption option)
{
	return config_data.GetPath(option);
}

unsigned
config_get_positive(ConfigOption option, unsigned default_value)
{
	return config_data.GetPositive(option, default_value);
}

bool
config_get_bool(ConfigOption option, bool default_value)
{
	return config_data.GetBool(option, default_value);
}
