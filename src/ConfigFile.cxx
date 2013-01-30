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
#include "conf.h"
#include "ConfigTemplates.hxx"
#include "ConfigParser.hxx"

extern "C" {
#include "utils.h"
#include "string_util.h"
#include "tokenizer.h"
}

#include "fs/Path.hxx"
#include "fs/FileSystem.hxx"
#include "mpd_error.h"

#include <glib.h>

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "config"

#define MAX_STRING_SIZE	MPD_PATH_MAX+80

#define CONF_COMMENT		'#'

static ConfigData config_data;

static void
config_param_free_callback(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
	struct config_param *param = (struct config_param *)data;

	config_param_free(param);
}

void config_global_finish(void)
{
	for (auto i : config_data.params) {
		g_slist_foreach(i, config_param_free_callback, NULL);
		g_slist_free(i);
	}
}

void config_global_init(void)
{
}

static void
config_param_check(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
	struct config_param *param = (struct config_param *)data;

	if (!param->used)
		/* this whole config_param was not queried at all -
		   the feature might be disabled at compile time?
		   Silently ignore it here. */
		return;

	for (unsigned i = 0; i < param->num_block_params; i++) {
		struct block_param *bp = &param->block_params[i];

		if (!bp->used)
			g_warning("option '%s' on line %i was not recognized",
				  bp->name, bp->line);
	}
}

void config_global_check(void)
{
	for (auto i : config_data.params)
		g_slist_foreach(i, config_param_check, NULL);
}

static bool
config_read_name_value(struct config_param *param, char *input, unsigned line,
		       GError **error_r)
{
	const char *name = tokenizer_next_word(&input, error_r);
	if (name == NULL) {
		assert(*input != 0);
		return false;
	}

	const char *value = tokenizer_next_string(&input, error_r);
	if (value == NULL) {
		if (*input == 0) {
			assert(error_r == NULL || *error_r == NULL);
			g_set_error(error_r, config_quark(), 0,
				    "Value missing");
		} else {
			assert(error_r == NULL || *error_r != NULL);
		}

		return false;
	}

	if (*input != 0 && *input != CONF_COMMENT) {
		g_set_error(error_r, config_quark(), 0,
			    "Unknown tokens after value");
		return false;
	}

	const struct block_param *bp = config_get_block_param(param, name);
	if (bp != NULL) {
		g_set_error(error_r, config_quark(), 0,
			    "\"%s\" is duplicate, first defined on line %i",
			    name, bp->line);
		return false;
	}

	config_add_block_param(param, name, value, line);
	return true;
}

static struct config_param *
config_read_block(FILE *fp, int *count, char *string, GError **error_r)
{
	struct config_param *ret = config_new_param(NULL, *count);
	GError *error = NULL;

	while (true) {
		char *line;

		line = fgets(string, MAX_STRING_SIZE, fp);
		if (line == NULL) {
			config_param_free(ret);
			g_set_error(error_r, config_quark(), 0,
				    "Expected '}' before end-of-file");
			return NULL;
		}

		(*count)++;
		line = strchug_fast(line);
		if (*line == 0 || *line == CONF_COMMENT)
			continue;

		if (*line == '}') {
			/* end of this block; return from the function
			   (and from this "while" loop) */

			line = strchug_fast(line + 1);
			if (*line != 0 && *line != CONF_COMMENT) {
				config_param_free(ret);
				g_set_error(error_r, config_quark(), 0,
					    "line %i: Unknown tokens after '}'",
					    *count);
				return nullptr;
			}

			return ret;
		}

		/* parse name and value */

		if (!config_read_name_value(ret, line, *count, &error)) {
			assert(*line != 0);
			config_param_free(ret);
			g_propagate_prefixed_error(error_r, error,
						   "line %i: ", *count);
			return NULL;
		}
	}
}

bool
ReadConfigFile(const Path &path, GError **error_r)
{
	assert(!path.IsNull());
	const std::string path_utf8 = path.ToUTF8();

	FILE *fp;
	char string[MAX_STRING_SIZE + 1];
	int count = 0;
	struct config_param *param;

	g_debug("loading file %s", path_utf8.c_str());

	if (!(fp = FOpen(path, "r"))) {
		g_set_error(error_r, config_quark(), errno,
			    "Failed to open %s: %s",
			    path_utf8.c_str(), g_strerror(errno));
		return false;
	}

	while (fgets(string, MAX_STRING_SIZE, fp)) {
		char *line;
		const char *name, *value;
		GError *error = NULL;

		count++;

		line = strchug_fast(string);
		if (*line == 0 || *line == CONF_COMMENT)
			continue;

		/* the first token in each line is the name, followed
		   by either the value or '{' */

		name = tokenizer_next_word(&line, &error);
		if (name == NULL) {
			assert(*line != 0);
			g_propagate_prefixed_error(error_r, error,
						   "line %i: ", count);
			fclose(fp);
			return false;
		}

		/* get the definition of that option, and check the
		   "repeatable" flag */

		const ConfigOption o = ParseConfigOptionName(name);
		if (o == CONF_MAX) {
			g_set_error(error_r, config_quark(), 0,
				    "unrecognized parameter in config file at "
				    "line %i: %s\n", count, name);
			fclose(fp);
			return false;
		}

		const unsigned i = ParseConfigOptionName(name);
		const ConfigTemplate &option = config_templates[i];
		GSList *&params = config_data.params[i];

		if (params != NULL && !option.repeatable) {
			param = (struct config_param *)params->data;
			g_set_error(error_r, config_quark(), 0,
				    "config parameter \"%s\" is first defined "
				    "on line %i and redefined on line %i\n",
				    name, param->line, count);
			fclose(fp);
			return false;
		}

		/* now parse the block or the value */

		if (option.block) {
			/* it's a block, call config_read_block() */

			if (*line != '{') {
				g_set_error(error_r, config_quark(), 0,
					    "line %i: '{' expected", count);
				fclose(fp);
				return false;
			}

			line = strchug_fast(line + 1);
			if (*line != 0 && *line != CONF_COMMENT) {
				g_set_error(error_r, config_quark(), 0,
					    "line %i: Unknown tokens after '{'",
					    count);
				fclose(fp);
				return false;
			}

			param = config_read_block(fp, &count, string, error_r);
			if (param == NULL) {
				fclose(fp);
				return false;
			}
		} else {
			/* a string value */

			value = tokenizer_next_string(&line, &error);
			if (value == NULL) {
				if (*line == 0)
					g_set_error(error_r, config_quark(), 0,
						    "line %i: Value missing",
						    count);
				else {
					g_set_error(error_r, config_quark(), 0,
						    "line %i: %s", count,
						    error->message);
					g_error_free(error);
				}

				fclose(fp);
				return false;
			}

			if (*line != 0 && *line != CONF_COMMENT) {
				g_set_error(error_r, config_quark(), 0,
					    "line %i: Unknown tokens after value",
					    count);
				fclose(fp);
				return false;
			}

			param = config_new_param(value, count);
		}

		params = g_slist_append(params, param);
	}
	fclose(fp);

	return true;
}

const struct config_param *
config_get_next_param(ConfigOption option, const struct config_param * last)
{
	GSList *node = config_data.params[unsigned(option)];

	if (last) {
		node = g_slist_find(node, last);
		if (node == NULL)
			return NULL;

		node = g_slist_next(node);
	}

	if (node == NULL)
		return NULL;

	struct config_param *param = (struct config_param *)node->data;
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
