/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "ConfigGlobal.hxx"
#include "ConfigParser.hxx"
#include "ConfigData.hxx"
#include "ConfigFile.hxx"
#include "ConfigPath.hxx"
#include "mpd_error.h"

#include <glib.h>

#include <assert.h>
#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "config"

static ConfigData config_data;

void config_global_finish(void)
{
	for (auto i : config_data.params)
		delete i;
}

void config_global_init(void)
{
}

bool
ReadConfigFile(const Path &path, GError **error_r)
{
	return ReadConfigFile(config_data, path, error_r);
}

static void
Check(const config_param *param)
{
	if (!param->used)
		/* this whole config_param was not queried at all -
		   the feature might be disabled at compile time?
		   Silently ignore it here. */
		return;

	for (const auto &i : param->block_params) {
		if (!i.used)
			g_warning("option '%s' on line %i was not recognized",
				  i.name.c_str(), i.line);
	}
}

void config_global_check(void)
{
	for (auto i : config_data.params)
		for (const config_param *p = i; p != nullptr; p = p->next)
			Check(p);
}

const struct config_param *
config_get_next_param(ConfigOption option, const struct config_param * last)
{
	config_param *param = last != nullptr
		? last->next
		: config_data.params[unsigned(option)];
	if (param != nullptr)
		param->used = true;
	return param;
}

const char *
config_get_string(ConfigOption option, const char *default_value)
{
	const struct config_param *param = config_get_param(option);

	if (param == NULL)
		return default_value;

	return param->value;
}

char *
config_dup_path(ConfigOption option, GError **error_r)
{
	assert(error_r != NULL);
	assert(*error_r == NULL);

	const struct config_param *param = config_get_param(option);
	if (param == NULL)
		return NULL;

	char *path = parsePath(param->value, error_r);
	if (G_UNLIKELY(path == NULL))
		g_prefix_error(error_r,
			       "Invalid path at line %i: ",
			       param->line);

	return path;
}

unsigned
config_get_unsigned(ConfigOption option, unsigned default_value)
{
	const struct config_param *param = config_get_param(option);
	long value;
	char *endptr;

	if (param == NULL)
		return default_value;

	value = strtol(param->value, &endptr, 0);
	if (*endptr != 0 || value < 0)
		MPD_ERROR("Not a valid non-negative number in line %i",
			  param->line);

	return (unsigned)value;
}

unsigned
config_get_positive(ConfigOption option, unsigned default_value)
{
	const struct config_param *param = config_get_param(option);
	long value;
	char *endptr;

	if (param == NULL)
		return default_value;

	value = strtol(param->value, &endptr, 0);
	if (*endptr != 0)
		MPD_ERROR("Not a valid number in line %i", param->line);

	if (value <= 0)
		MPD_ERROR("Not a positive number in line %i", param->line);

	return (unsigned)value;
}

bool
config_get_bool(ConfigOption option, bool default_value)
{
	const struct config_param *param = config_get_param(option);
	bool success, value;

	if (param == NULL)
		return default_value;

	success = get_bool(param->value, &value);
	if (!success)
		MPD_ERROR("Expected boolean value (yes, true, 1) or "
			  "(no, false, 0) on line %i\n",
			  param->line);

	return value;
}
