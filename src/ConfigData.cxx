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

#include "ConfigData.hxx"
#include "ConfigParser.hxx"
#include "mpd_error.h"

extern "C" {
#include "utils.h"
}

#include <glib.h>

#include <assert.h>
#include <string.h>
#include <stdlib.h>

struct config_param *
config_new_param(const char *value, int line)
{
	config_param *ret = new config_param();

	if (!value)
		ret->value = NULL;
	else
		ret->value = g_strdup(value);

	ret->line = line;

	ret->used = false;

	return ret;
}

void
config_param_free(struct config_param *param)
{
	g_free(param->value);

	for (auto &i : param->block_params) {
		g_free(i.name);
		g_free(i.value);
	}

	delete param;
}

void
config_add_block_param(struct config_param * param, const char *name,
		       const char *value, int line)
{
	assert(config_get_block_param(param, name) == NULL);

	param->block_params.push_back(block_param());
	struct block_param *bp = &param->block_params.back();

	bp->name = g_strdup(name);
	bp->value = g_strdup(value);
	bp->line = line;
	bp->used = false;
}

const struct block_param *
config_get_block_param(const struct config_param * param, const char *name)
{
	if (param == NULL)
		return NULL;

	for (auto &i : param->block_params) {
		if (0 == strcmp(name, i.name)) {
			i.used = true;
			return &i;
		}
	}

	return NULL;
}

const char *
config_get_block_string(const struct config_param *param, const char *name,
			const char *default_value)
{
	const struct block_param *bp = config_get_block_param(param, name);

	if (bp == NULL)
		return default_value;

	return bp->value;
}

char *
config_dup_block_string(const struct config_param *param, const char *name,
			const char *default_value)
{
	return g_strdup(config_get_block_string(param, name, default_value));
}

char *
config_dup_block_path(const struct config_param *param, const char *name,
		      GError **error_r)
{
	assert(error_r != NULL);
	assert(*error_r == NULL);

	const struct block_param *bp = config_get_block_param(param, name);
	if (bp == NULL)
		return NULL;

	char *path = parsePath(bp->value, error_r);
	if (G_UNLIKELY(path == NULL))
		g_prefix_error(error_r,
			       "Invalid path in \"%s\" at line %i: ",
			       name, bp->line);

	return path;
}

unsigned
config_get_block_unsigned(const struct config_param *param, const char *name,
			  unsigned default_value)
{
	const struct block_param *bp = config_get_block_param(param, name);
	long value;
	char *endptr;

	if (bp == NULL)
		return default_value;

	value = strtol(bp->value, &endptr, 0);
	if (*endptr != 0)
		MPD_ERROR("Not a valid number in line %i", bp->line);

	if (value < 0)
		MPD_ERROR("Not a positive number in line %i", bp->line);

	return (unsigned)value;
}

bool
config_get_block_bool(const struct config_param *param, const char *name,
		      bool default_value)
{
	const struct block_param *bp = config_get_block_param(param, name);
	bool success, value;

	if (bp == NULL)
		return default_value;

	success = get_bool(bp->value, &value);
	if (!success)
		MPD_ERROR("%s is not a boolean value (yes, true, 1) or "
			  "(no, false, 0) on line %i\n",
			  name, bp->line);

	return value;
}
