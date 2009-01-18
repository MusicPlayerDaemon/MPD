/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "conf.h"
#include "utils.h"
#include "buffer2array.h"
#include "path.h"

#include <glib.h>

#include <string.h>
#include <stdio.h>
#include <errno.h>

#define MAX_STRING_SIZE	MPD_PATH_MAX+80

#define CONF_COMMENT		'#'
#define CONF_BLOCK_BEGIN	"{"
#define CONF_BLOCK_END		"}"

#define CONF_REPEATABLE_MASK	0x01
#define CONF_BLOCK_MASK		0x02
#define CONF_LINE_TOKEN_MAX	3

struct config_entry {
	const char *name;
	unsigned char mask;

	GSList *params;
};

static GSList *config_entries;

static int get_bool(const char *value)
{
	const char **x;
	static const char *t[] = { "yes", "true", "1", NULL };
	static const char *f[] = { "no", "false", "0", NULL };

	for (x = t; *x; x++) {
		if (!strcasecmp(*x, value))
			return 1;
	}
	for (x = f; *x; x++) {
		if (!strcasecmp(*x, value))
			return 0;
	}
	return CONF_BOOL_INVALID;
}

struct config_param *
newConfigParam(const char *value, int line)
{
	struct config_param *ret = g_new(struct config_param, 1);

	if (!value)
		ret->value = NULL;
	else
		ret->value = g_strdup(value);

	ret->line = line;

	ret->num_block_params = 0;
	ret->block_params = NULL;

	return ret;
}

void
config_param_free(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
	struct config_param *param = data;
	int i;

	g_free(param->value);

	for (i = 0; i < param->num_block_params; i++) {
		g_free(param->block_params[i].name);
		g_free(param->block_params[i].value);
	}

	if (param->num_block_params)
		g_free(param->block_params);

	g_free(param);
}

static struct config_entry *
newConfigEntry(const char *name, int repeatable, int block)
{
	struct config_entry *ret = g_new(struct config_entry, 1);

	ret->name = name;
	ret->mask = 0;
	ret->params = NULL;

	if (repeatable)
		ret->mask |= CONF_REPEATABLE_MASK;
	if (block)
		ret->mask |= CONF_BLOCK_MASK;

	return ret;
}

static void
config_entry_free(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
	struct config_entry *entry = data;

	g_slist_foreach(entry->params, config_param_free, NULL);
	g_slist_free(entry->params);

	g_free(entry);
}

static struct config_entry *
config_entry_get(const char *name)
{
	GSList *list;

	for (list = config_entries; list != NULL;
	     list = g_slist_next(list)) {
		struct config_entry *entry = list->data;
		if (strcmp(entry->name, name) == 0)
			return entry;
	}

	return NULL;
}

static void registerConfigParam(const char *name, int repeatable, int block)
{
	struct config_entry *entry;

	entry = config_entry_get(name);
	if (entry != NULL)
		g_error("config parameter \"%s\" already registered\n", name);

	entry = newConfigEntry(name, repeatable, block);
	config_entries = g_slist_prepend(config_entries, entry);
}

void config_global_finish(void)
{
	g_slist_foreach(config_entries, config_entry_free, NULL);
	g_slist_free(config_entries);
}

void config_global_init(void)
{
	config_entries = NULL;

	/* registerConfigParam(name,                   repeatable, block); */
	registerConfigParam(CONF_MUSIC_DIR,                     0,     0);
	registerConfigParam(CONF_PLAYLIST_DIR,                  0,     0);
	registerConfigParam(CONF_FOLLOW_INSIDE_SYMLINKS,        0,     0);
	registerConfigParam(CONF_FOLLOW_OUTSIDE_SYMLINKS,       0,     0);
	registerConfigParam(CONF_DB_FILE,                       0,     0);
	registerConfigParam(CONF_LOG_FILE,                      0,     0);
	registerConfigParam(CONF_ERROR_FILE,                    0,     0);
	registerConfigParam(CONF_PID_FILE,                      0,     0);
	registerConfigParam(CONF_STATE_FILE,                    0,     0);
	registerConfigParam(CONF_USER,                          0,     0);
	registerConfigParam(CONF_BIND_TO_ADDRESS,               1,     0);
	registerConfigParam(CONF_PORT,                          0,     0);
	registerConfigParam(CONF_LOG_LEVEL,                     0,     0);
	registerConfigParam(CONF_ZEROCONF_NAME,                 0,     0);
	registerConfigParam(CONF_ZEROCONF_ENABLED,              0,     0);
	registerConfigParam(CONF_PASSWORD,                      1,     0);
	registerConfigParam(CONF_DEFAULT_PERMS,                 0,     0);
	registerConfigParam(CONF_AUDIO_OUTPUT,                  1,     1);
	registerConfigParam(CONF_AUDIO_OUTPUT_FORMAT,           0,     0);
	registerConfigParam(CONF_MIXER_TYPE,                    0,     0);
	registerConfigParam(CONF_MIXER_DEVICE,                  0,     0);
	registerConfigParam(CONF_MIXER_CONTROL,                 0,     0);
	registerConfigParam(CONF_REPLAYGAIN,                    0,     0);
	registerConfigParam(CONF_REPLAYGAIN_PREAMP,             0,     0);
	registerConfigParam(CONF_VOLUME_NORMALIZATION,          0,     0);
	registerConfigParam(CONF_SAMPLERATE_CONVERTER,          0,     0);
	registerConfigParam(CONF_AUDIO_BUFFER_SIZE,             0,     0);
	registerConfigParam(CONF_BUFFER_BEFORE_PLAY,            0,     0);
	registerConfigParam(CONF_HTTP_PROXY_HOST,               0,     0);
	registerConfigParam(CONF_HTTP_PROXY_PORT,               0,     0);
	registerConfigParam(CONF_HTTP_PROXY_USER,               0,     0);
	registerConfigParam(CONF_HTTP_PROXY_PASSWORD,           0,     0);
	registerConfigParam(CONF_CONN_TIMEOUT,                  0,     0);
	registerConfigParam(CONF_MAX_CONN,                      0,     0);
	registerConfigParam(CONF_MAX_PLAYLIST_LENGTH,           0,     0);
	registerConfigParam(CONF_MAX_COMMAND_LIST_SIZE,         0,     0);
	registerConfigParam(CONF_MAX_OUTPUT_BUFFER_SIZE,        0,     0);
	registerConfigParam(CONF_FS_CHARSET,                    0,     0);
	registerConfigParam(CONF_ID3V1_ENCODING,                0,     0);
	registerConfigParam(CONF_METADATA_TO_USE,               0,     0);
	registerConfigParam(CONF_SAVE_ABSOLUTE_PATHS,           0,     0);
	registerConfigParam(CONF_GAPLESS_MP3_PLAYBACK,          0,     0);
}

void
addBlockParam(struct config_param * param, const char *name, const char *value,
	      int line)
{
	struct block_param *bp;

	param->num_block_params++;

	param->block_params = g_realloc(param->block_params,
					param->num_block_params *
					sizeof(param->block_params[0]));

	bp = &param->block_params[param->num_block_params - 1];

	bp->name = g_strdup(name);
	bp->value = g_strdup(value);
	bp->line = line;
}

static struct config_param *
config_read_fileigBlock(FILE * fp, int *count, char *string)
{
	struct config_param *ret = newConfigParam(NULL, *count);

	int i;
	int numberOfArgs;
	int argsMinusComment;

	while (fgets(string, MAX_STRING_SIZE, fp)) {
		char *array[CONF_LINE_TOKEN_MAX] = { NULL };

		(*count)++;

		numberOfArgs = buffer2array(string, array, CONF_LINE_TOKEN_MAX);

		for (i = 0; i < numberOfArgs; i++) {
			if (array[i][0] == CONF_COMMENT)
				break;
		}

		argsMinusComment = i;

		if (0 == argsMinusComment) {
			continue;
		}

		if (1 == argsMinusComment &&
		    0 == strcmp(array[0], CONF_BLOCK_END)) {
			break;
		}

		if (2 != argsMinusComment) {
			g_error("improperly formatted config file at line %i:"
				" %s\n", *count, string);
		}

		if (0 == strcmp(array[0], CONF_BLOCK_BEGIN) ||
		    0 == strcmp(array[1], CONF_BLOCK_BEGIN) ||
		    0 == strcmp(array[0], CONF_BLOCK_END) ||
		    0 == strcmp(array[1], CONF_BLOCK_END)) {
			g_error("improperly formatted config file at line %i: %s "
				"in block beginning at line %i\n",
				*count, string, ret->line);
		}

		addBlockParam(ret, array[0], array[1], *count);
	}

	return ret;
}

void config_read_file(const char *file)
{
	FILE *fp;
	char string[MAX_STRING_SIZE + 1];
	int i;
	int numberOfArgs;
	int argsMinusComment;
	int count = 0;
	struct config_entry *entry;
	struct config_param *param;

	if (!(fp = fopen(file, "r"))) {
		g_error("problems opening file %s for reading: %s\n",
			file, strerror(errno));
	}

	while (fgets(string, MAX_STRING_SIZE, fp)) {
		char *array[CONF_LINE_TOKEN_MAX] = { NULL };

		count++;

		numberOfArgs = buffer2array(string, array, CONF_LINE_TOKEN_MAX);

		for (i = 0; i < numberOfArgs; i++) {
			if (array[i][0] == CONF_COMMENT)
				break;
		}

		argsMinusComment = i;

		if (0 == argsMinusComment) {
			continue;
		}

		if (2 != argsMinusComment) {
			g_error("improperly formatted config file at line %i:"
				" %s\n", count, string);
		}

		entry = config_entry_get(array[0]);
		if (entry == NULL)
			g_error("unrecognized parameter in config file at "
				"line %i: %s\n", count, string);

		if (!(entry->mask & CONF_REPEATABLE_MASK) &&
		    entry->params != NULL) {
			param = entry->params->data;
			g_error("config parameter \"%s\" is first defined on "
				"line %i and redefined on line %i\n",
				array[0], param->line, count);
		}

		if (entry->mask & CONF_BLOCK_MASK) {
			if (0 != strcmp(array[1], CONF_BLOCK_BEGIN)) {
				g_error("improperly formatted config file at "
					"line %i: %s\n", count, string);
			}
			param = config_read_fileigBlock(fp, &count, string);
		} else
			param = newConfigParam(array[1], count);

		entry->params = g_slist_append(entry->params, param);
	}
	fclose(fp);
}

struct config_param *
config_get_next_param(const char *name, struct config_param * last)
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

	return param;
}

const char *
config_get_string(const char *name, const char *default_value)
{
	struct config_param *param = config_get_param(name);

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

struct block_param *
getBlockParam(struct config_param * param, const char *name)
{
	struct block_param *ret = NULL;
	int i;

	for (i = 0; i < param->num_block_params; i++) {
		if (0 == strcmp(name, param->block_params[i].name)) {
			if (ret) {
				g_warning("\"%s\" first defined on line %i, and "
					  "redefined on line %i\n", name,
					  ret->line, param->block_params[i].line);
			}
			ret = param->block_params + i;
		}
	}

	return ret;
}

bool config_get_bool(const char *name, bool default_value)
{
	struct config_param *param = config_get_param(name);
	int value;

	if (param == NULL)
		return default_value;

	value = get_bool(param->value);
	if (value == CONF_BOOL_INVALID)
		g_error("%s is not a boolean value (yes, true, 1) or "
			"(no, false, 0) on line %i\n",
			name, param->line);

	if (value == CONF_BOOL_UNSET)
		return default_value;

	return !!value;
}

const char *
config_get_block_string(struct config_param *param, const char *name,
			const char *default_value)
{
	struct block_param *bp = getBlockParam(param, name);

	if (bp == NULL)
		return default_value;

	return bp->value;
}

bool
config_get_block_bool(struct config_param *param, const char *name,
		      bool default_value)
{
	struct block_param *bp = getBlockParam(param, name);
	int value;

	if (bp == NULL)
		return default_value;

	value = get_bool(bp->value);
	if (value == CONF_BOOL_INVALID)
		g_error("%s is not a boolean value (yes, true, 1) or "
			"(no, false, 0) on line %i\n",
			name, bp->line);

	if (value == CONF_BOOL_UNSET)
		return default_value;

	return !!value;
}
