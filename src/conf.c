/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
#include "utils.h"
#include "tokenizer.h"
#include "path.h"
#include "glib_compat.h"

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

struct config_entry {
	const char *const name;
	const bool repeatable;
	const bool block;

	GSList *params;
};

static struct config_entry config_entries[] = {
	{ .name = CONF_MUSIC_DIR, false, false },
	{ .name = CONF_PLAYLIST_DIR, false, false },
	{ .name = CONF_FOLLOW_INSIDE_SYMLINKS, false, false },
	{ .name = CONF_FOLLOW_OUTSIDE_SYMLINKS, false, false },
	{ .name = CONF_DB_FILE, false, false },
	{ .name = CONF_STICKER_FILE, false, false },
	{ .name = CONF_LOG_FILE, false, false },
	{ .name = CONF_PID_FILE, false, false },
	{ .name = CONF_STATE_FILE, false, false },
	{ .name = CONF_USER, false, false },
	{ .name = CONF_GROUP, false, false },
	{ .name = CONF_BIND_TO_ADDRESS, true, false },
	{ .name = CONF_PORT, false, false },
	{ .name = CONF_LOG_LEVEL, false, false },
	{ .name = CONF_ZEROCONF_NAME, false, false },
	{ .name = CONF_ZEROCONF_ENABLED, false, false },
	{ .name = CONF_PASSWORD, true, false },
	{ .name = CONF_DEFAULT_PERMS, false, false },
	{ .name = CONF_AUDIO_OUTPUT, true, true },
	{ .name = CONF_AUDIO_OUTPUT_FORMAT, false, false },
	{ .name = CONF_MIXER_TYPE, false, false },
	{ .name = CONF_REPLAYGAIN, false, false },
	{ .name = CONF_REPLAYGAIN_PREAMP, false, false },
	{ .name = CONF_REPLAYGAIN_MISSING_PREAMP, false, false },
	{ .name = CONF_VOLUME_NORMALIZATION, false, false },
	{ .name = CONF_SAMPLERATE_CONVERTER, false, false },
	{ .name = CONF_AUDIO_BUFFER_SIZE, false, false },
	{ .name = CONF_BUFFER_BEFORE_PLAY, false, false },
	{ .name = CONF_HTTP_PROXY_HOST, false, false },
	{ .name = CONF_HTTP_PROXY_PORT, false, false },
	{ .name = CONF_HTTP_PROXY_USER, false, false },
	{ .name = CONF_HTTP_PROXY_PASSWORD, false, false },
	{ .name = CONF_CONN_TIMEOUT, false, false },
	{ .name = CONF_MAX_CONN, false, false },
	{ .name = CONF_MAX_PLAYLIST_LENGTH, false, false },
	{ .name = CONF_MAX_COMMAND_LIST_SIZE, false, false },
	{ .name = CONF_MAX_OUTPUT_BUFFER_SIZE, false, false },
	{ .name = CONF_FS_CHARSET, false, false },
	{ .name = CONF_ID3V1_ENCODING, false, false },
	{ .name = CONF_METADATA_TO_USE, false, false },
	{ .name = CONF_SAVE_ABSOLUTE_PATHS, false, false },
	{ .name = CONF_DECODER, true, true },
	{ .name = CONF_INPUT, true, true },
	{ .name = CONF_GAPLESS_MP3_PLAYBACK, false, false },
	{ .name = CONF_PLAYLIST_PLUGIN, true, true },
	{ .name = CONF_AUTO_UPDATE, false, false },
	{ .name = "filter", true, true },
};

static bool
get_bool(const char *value, bool *value_r)
{
	static const char *t[] = { "yes", "true", "1", NULL };
	static const char *f[] = { "no", "false", "0", NULL };

	if (string_array_contains(t, value)) {
		*value_r = true;
		return true;
	}

	if (string_array_contains(f, value)) {
		*value_r = false;
		return true;
	}

	return false;
}

struct config_param *
config_new_param(const char *value, int line)
{
	struct config_param *ret = g_new(struct config_param, 1);

	if (!value)
		ret->value = NULL;
	else
		ret->value = g_strdup(value);

	ret->line = line;

	ret->num_block_params = 0;
	ret->block_params = NULL;
	ret->used = false;

	return ret;
}

static void
config_param_free(struct config_param *param)
{
	g_free(param->value);

	for (unsigned i = 0; i < param->num_block_params; i++) {
		g_free(param->block_params[i].name);
		g_free(param->block_params[i].value);
	}

	if (param->num_block_params)
		g_free(param->block_params);

	g_free(param);
}

static void
config_param_free_callback(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
	struct config_param *param = data;

	config_param_free(param);
}

static struct config_entry *
config_entry_get(const char *name)
{
	for (unsigned i = 0; i < G_N_ELEMENTS(config_entries); ++i) {
		struct config_entry *entry = &config_entries[i];
		if (strcmp(entry->name, name) == 0)
			return entry;
	}

	return NULL;
}

void config_global_finish(void)
{
	for (unsigned i = 0; i < G_N_ELEMENTS(config_entries); ++i) {
		struct config_entry *entry = &config_entries[i];

		g_slist_foreach(entry->params,
				config_param_free_callback, NULL);
		g_slist_free(entry->params);
	}
}

void config_global_init(void)
{
}

static void
config_param_check(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
	struct config_param *param = data;

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
	for (unsigned i = 0; i < G_N_ELEMENTS(config_entries); ++i) {
		struct config_entry *entry = &config_entries[i];

		g_slist_foreach(entry->params, config_param_check, NULL);
	}
}

bool
config_add_block_param(struct config_param * param, const char *name,
		       const char *value, int line, GError **error_r)
{
	struct block_param *bp;

	bp = config_get_block_param(param, name);
	if (bp != NULL) {
		g_set_error(error_r, config_quark(), 0,
			    "\"%s\" first defined on line %i, and "
			    "redefined on line %i\n", name,
			    bp->line, line);
		return false;
	}

	param->num_block_params++;

	param->block_params = g_realloc(param->block_params,
					param->num_block_params *
					sizeof(param->block_params[0]));

	bp = &param->block_params[param->num_block_params - 1];

	bp->name = g_strdup(name);
	bp->value = g_strdup(value);
	bp->line = line;
	bp->used = false;

	return true;
}

static struct config_param *
config_read_block(FILE *fp, int *count, char *string, GError **error_r)
{
	struct config_param *ret = config_new_param(NULL, *count);
	GError *error = NULL;
	bool success;

	while (true) {
		char *line;
		const char *name, *value;

		line = fgets(string, MAX_STRING_SIZE, fp);
		if (line == NULL) {
			config_param_free(ret);
			g_set_error(error_r, config_quark(), 0,
				    "Expected '}' before end-of-file");
			return NULL;
		}

		(*count)++;
		line = g_strchug(line);
		if (*line == 0 || *line == CONF_COMMENT)
			continue;

		if (*line == '}') {
			/* end of this block; return from the function
			   (and from this "while" loop) */

			line = g_strchug(line + 1);
			if (*line != 0 && *line != CONF_COMMENT) {
				config_param_free(ret);
				g_set_error(error_r, config_quark(), 0,
					    "line %i: Unknown tokens after '}'",
					    *count);
				return false;
			}

			return ret;
		}

		/* parse name and value */

		name = tokenizer_next_word(&line, &error);
		if (name == NULL) {
			assert(*line != 0);
			config_param_free(ret);
			g_propagate_prefixed_error(error_r, error,
						   "line %i: ", *count);
			return NULL;
		}

		value = tokenizer_next_string(&line, &error);
		if (value == NULL) {
			config_param_free(ret);
			if (*line == 0)
				g_set_error(error_r, config_quark(), 0,
					    "line %i: Value missing", *count);
			else
				g_propagate_prefixed_error(error_r, error,
							   "line %i: ",
							   *count);
			return NULL;
		}

		if (*line != 0 && *line != CONF_COMMENT) {
			config_param_free(ret);
			g_set_error(error_r, config_quark(), 0,
				    "line %i: Unknown tokens after value",
				    *count);
			return NULL;
		}

		success = config_add_block_param(ret, name, value, *count,
						 error_r);
		if (!success) {
			config_param_free(ret);
			return false;
		}
	}
}

bool
config_read_file(const char *file, GError **error_r)
{
	FILE *fp;
	char string[MAX_STRING_SIZE + 1];
	int count = 0;
	struct config_entry *entry;
	struct config_param *param;

	g_debug("loading file %s", file);

	if (!(fp = fopen(file, "r"))) {
		g_set_error(error_r, config_quark(), errno,
			    "Failed to open %s: %s",
			    file, strerror(errno));
		return false;
	}

	while (fgets(string, MAX_STRING_SIZE, fp)) {
		char *line;
		const char *name, *value;
		GError *error = NULL;

		count++;

		line = g_strchug(string);
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

		entry = config_entry_get(name);
		if (entry == NULL) {
			g_set_error(error_r, config_quark(), 0,
				    "unrecognized parameter in config file at "
				    "line %i: %s\n", count, name);
			return false;
		}

		if (entry->params != NULL && !entry->repeatable) {
			param = entry->params->data;
			g_set_error(error_r, config_quark(), 0,
				    "config parameter \"%s\" is first defined "
				    "on line %i and redefined on line %i\n",
				    name, param->line, count);
			return false;
		}

		/* now parse the block or the value */

		if (entry->block) {
			/* it's a block, call config_read_block() */

			if (*line != '{') {
				g_set_error(error_r, config_quark(), 0,
					    "line %i: '{' expected", count);
				return false;
			}

			line = g_strchug(line + 1);
			if (*line != 0 && *line != CONF_COMMENT) {
				g_set_error(error_r, config_quark(), 0,
					    "line %i: Unknown tokens after '{'",
					    count);
				return false;
			}

			param = config_read_block(fp, &count, string, error_r);
			if (param == NULL)
				return false;
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

			param = config_new_param(value, count);
		}

		entry->params = g_slist_append(entry->params, param);
	}
	fclose(fp);

	return true;
}

struct config_param *
config_get_next_param(const char *name, const struct config_param * last)
{
	struct config_entry *entry;
	GSList *node;
	struct config_param *param;

	entry = config_entry_get(name);
	if (entry == NULL)
		return NULL;

	node = entry->params;

	if (last) {
		node = g_slist_find(node, last);
		if (node == NULL)
			return NULL;

		node = g_slist_next(node);
	}

	if (node == NULL)
		return NULL;

	param = node->data;
	param->used = true;
	return param;
}

const char *
config_get_string(const char *name, const char *default_value)
{
	const struct config_param *param = config_get_param(name);

	if (param == NULL)
		return default_value;

	return param->value;
}

const char *
config_get_path(const char *name)
{
	struct config_param *param = config_get_param(name);
	char *path;

	if (param == NULL)
		return NULL;

	path = parsePath(param->value);
	if (path == NULL)
		g_error("error parsing \"%s\" at line %i\n",
			name, param->line);

	g_free(param->value);
	return param->value = path;
}

unsigned
config_get_positive(const char *name, unsigned default_value)
{
	const struct config_param *param = config_get_param(name);
	long value;
	char *endptr;

	if (param == NULL)
		return default_value;

	value = strtol(param->value, &endptr, 0);
	if (*endptr != 0)
		g_error("Not a valid number in line %i", param->line);

	if (value <= 0)
		g_error("Not a positive number in line %i", param->line);

	return (unsigned)value;
}

struct block_param *
config_get_block_param(const struct config_param * param, const char *name)
{
	if (param == NULL)
		return NULL;

	for (unsigned i = 0; i < param->num_block_params; i++) {
		if (0 == strcmp(name, param->block_params[i].name)) {
			struct block_param *bp = &param->block_params[i];
			bp->used = true;
			return bp;
		}
	}

	return NULL;
}

bool config_get_bool(const char *name, bool default_value)
{
	const struct config_param *param = config_get_param(name);
	bool success, value;

	if (param == NULL)
		return default_value;

	success = get_bool(param->value, &value);
	if (!success)
		g_error("%s is not a boolean value (yes, true, 1) or "
			"(no, false, 0) on line %i\n",
			name, param->line);

	return value;
}

const char *
config_get_block_string(const struct config_param *param, const char *name,
			const char *default_value)
{
	struct block_param *bp = config_get_block_param(param, name);

	if (bp == NULL)
		return default_value;

	return bp->value;
}

unsigned
config_get_block_unsigned(const struct config_param *param, const char *name,
			  unsigned default_value)
{
	struct block_param *bp = config_get_block_param(param, name);
	long value;
	char *endptr;

	if (bp == NULL)
		return default_value;

	value = strtol(bp->value, &endptr, 0);
	if (*endptr != 0)
		g_error("Not a valid number in line %i", bp->line);

	if (value < 0)
		g_error("Not a positive number in line %i", bp->line);

	return (unsigned)value;
}

bool
config_get_block_bool(const struct config_param *param, const char *name,
		      bool default_value)
{
	struct block_param *bp = config_get_block_param(param, name);
	bool success, value;

	if (bp == NULL)
		return default_value;

	success = get_bool(bp->value, &value);
	if (!success)
		g_error("%s is not a boolean value (yes, true, 1) or "
			"(no, false, 0) on line %i\n",
			name, bp->line);

	return value;
}
