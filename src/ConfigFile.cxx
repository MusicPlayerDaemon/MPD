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
#include "ConfigFile.hxx"
#include "ConfigQuark.hxx"
#include "ConfigData.hxx"
#include "ConfigTemplates.hxx"
#include "conf.h"

extern "C" {
#include "string_util.h"
#include "tokenizer.h"
}

#include "fs/Path.hxx"
#include "fs/FileSystem.hxx"

#include <glib.h>

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "config"

#define MAX_STRING_SIZE	MPD_PATH_MAX+80

#define CONF_COMMENT		'#'

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

	const struct block_param *bp = param->GetBlockParam(name);
	if (bp != NULL) {
		g_set_error(error_r, config_quark(), 0,
			    "\"%s\" is duplicate, first defined on line %i",
			    name, bp->line);
		return false;
	}

	param->AddBlockParam(name, value, line);
	return true;
}

static struct config_param *
config_read_block(FILE *fp, int *count, char *string, GError **error_r)
{
	struct config_param *ret = new config_param(*count);
	GError *error = NULL;

	while (true) {
		char *line;

		line = fgets(string, MAX_STRING_SIZE, fp);
		if (line == NULL) {
			delete ret;
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
				delete ret;
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
			delete ret;
			g_propagate_prefixed_error(error_r, error,
						   "line %i: ", *count);
			return NULL;
		}
	}
}

static bool
ReadConfigFile(ConfigData &config_data, FILE *fp, GError **error_r)
{
	assert(fp != nullptr);

	char string[MAX_STRING_SIZE + 1];
	int count = 0;
	struct config_param *param;

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
			return false;
		}

		/* get the definition of that option, and check the
		   "repeatable" flag */

		const ConfigOption o = ParseConfigOptionName(name);
		if (o == CONF_MAX) {
			g_set_error(error_r, config_quark(), 0,
				    "unrecognized parameter in config file at "
				    "line %i: %s\n", count, name);
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
			return false;
		}

		/* now parse the block or the value */

		if (option.block) {
			/* it's a block, call config_read_block() */

			if (*line != '{') {
				g_set_error(error_r, config_quark(), 0,
					    "line %i: '{' expected", count);
				return false;
			}

			line = strchug_fast(line + 1);
			if (*line != 0 && *line != CONF_COMMENT) {
				g_set_error(error_r, config_quark(), 0,
					    "line %i: Unknown tokens after '{'",
					    count);
				return false;
			}

			param = config_read_block(fp, &count, string, error_r);
			if (param == NULL) {
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

				return false;
			}

			if (*line != 0 && *line != CONF_COMMENT) {
				g_set_error(error_r, config_quark(), 0,
					    "line %i: Unknown tokens after value",
					    count);
				return false;
			}

			param = new config_param(value, count);
		}

		params = g_slist_append(params, param);
	}

	return true;
}

bool
ReadConfigFile(ConfigData &config_data, const Path &path, GError **error_r)
{
	assert(!path.IsNull());
	const std::string path_utf8 = path.ToUTF8();

	g_debug("loading file %s", path_utf8.c_str());

	FILE *fp = FOpen(path, "r");
	if (fp == nullptr) {
		g_set_error(error_r, config_quark(), errno,
			    "Failed to open %s: %s",
			    path_utf8.c_str(), g_strerror(errno));
		return false;
	}

	bool result = ReadConfigFile(config_data, fp, error_r);
	fclose(fp);
	return result;
}
