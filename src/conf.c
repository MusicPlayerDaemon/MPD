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

#include "log.h"

#include "utils.h"
#include "buffer2array.h"
#include "list.h"

#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <errno.h>

#define MAX_STRING_SIZE	MAXPATHLEN+80

#define CONF_COMMENT		'#'
#define CONF_BLOCK_BEGIN	"{"
#define CONF_BLOCK_END		"}"

#define CONF_REPEATABLE_MASK	0x01
#define CONF_BLOCK_MASK		0x02
#define CONF_LINE_TOKEN_MAX	3

typedef struct _configEntry {
	unsigned char mask;
	List *configParamList;
} ConfigEntry;

static List *configEntriesList;

static ConfigParam *newConfigParam(char *value, int line)
{
	ConfigParam *ret = xmalloc(sizeof(ConfigParam));

	if (!value)
		ret->value = NULL;
	else
		ret->value = xstrdup(value);

	ret->line = line;

	ret->numberOfBlockParams = 0;
	ret->blockParams = NULL;

	return ret;
}

static void freeConfigParam(ConfigParam * param)
{
	int i;

	if (param->value)
		free(param->value);

	for (i = 0; i < param->numberOfBlockParams; i++) {
		if (param->blockParams[i].name) {
			free(param->blockParams[i].name);
		}
		if (param->blockParams[i].value) {
			free(param->blockParams[i].value);
		}
	}

	if (param->numberOfBlockParams)
		free(param->blockParams);

	free(param);
}

static ConfigEntry *newConfigEntry(int repeatable, int block)
{
	ConfigEntry *ret = xmalloc(sizeof(ConfigEntry));

	ret->mask = 0;
	ret->configParamList =
	    makeList((ListFreeDataFunc *) freeConfigParam, 1);

	if (repeatable)
		ret->mask |= CONF_REPEATABLE_MASK;
	if (block)
		ret->mask |= CONF_BLOCK_MASK;

	return ret;
}

static void freeConfigEntry(ConfigEntry * entry)
{
	freeList(entry->configParamList);
	free(entry);
}

static void registerConfigParam(char *name, int repeatable, int block)
{
	ConfigEntry *entry;

	if (findInList(configEntriesList, name, NULL))
		FATAL("config parameter \"%s\" already registered\n", name);

	entry = newConfigEntry(repeatable, block);

	insertInList(configEntriesList, name, entry);
}

void finishConf(void)
{
	freeList(configEntriesList);
}

void initConf(void)
{
	configEntriesList = makeList((ListFreeDataFunc *) freeConfigEntry, 1);

	/* registerConfigParam(name,                   repeatable, block); */
	registerConfigParam(CONF_MUSIC_DIR,                     0,     0);
	registerConfigParam(CONF_PLAYLIST_DIR,                  0,     0);
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
	registerConfigParam(CONF_HTTP_BUFFER_SIZE,              0,     0);
	registerConfigParam(CONF_HTTP_PREBUFFER_SIZE,           0,     0);
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

static void addBlockParam(ConfigParam * param, char *name, char *value,
			  int line)
{
	param->numberOfBlockParams++;

	param->blockParams = xrealloc(param->blockParams,
				     param->numberOfBlockParams *
				     sizeof(BlockParam));

	param->blockParams[param->numberOfBlockParams - 1].name = xstrdup(name);
	param->blockParams[param->numberOfBlockParams - 1].value =
	    xstrdup(value);
	param->blockParams[param->numberOfBlockParams - 1].line = line;
}

static ConfigParam *readConfigBlock(FILE * fp, int *count, char *string)
{
	ConfigParam *ret = newConfigParam(NULL, *count);

	int i;
	int numberOfArgs;
	int argsMinusComment;

	while (myFgets(string, MAX_STRING_SIZE, fp)) {
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
			FATAL("improperly formatted config file at line %i:"
			      " %s\n", *count, string);
		}

		if (0 == strcmp(array[0], CONF_BLOCK_BEGIN) ||
		    0 == strcmp(array[1], CONF_BLOCK_BEGIN) ||
		    0 == strcmp(array[0], CONF_BLOCK_END) ||
		    0 == strcmp(array[1], CONF_BLOCK_END)) {
			FATAL("improperly formatted config file at line %i: %s\n"
			      "in block beginning at line %i\n",
			      *count, string, ret->line);
		}

		addBlockParam(ret, array[0], array[1], *count);
	}

	return ret;
}

void readConf(char *file)
{
	FILE *fp;
	char string[MAX_STRING_SIZE + 1];
	int i;
	int numberOfArgs;
	int argsMinusComment;
	int count = 0;
	ConfigEntry *entry;
	void *voidPtr;
	ConfigParam *param;

	if (!(fp = fopen(file, "r"))) {
		FATAL("problems opening file %s for reading: %s\n", file,
		      strerror(errno));
	}

	while (myFgets(string, MAX_STRING_SIZE, fp)) {
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
			FATAL("improperly formatted config file at line %i:"
			      " %s\n", count, string);
		}

		if (!findInList(configEntriesList, array[0], &voidPtr)) {
			FATAL("unrecognized parameter in config file at line "
			      "%i: %s\n", count, string);
		}

		entry = (ConfigEntry *) voidPtr;

		if (!(entry->mask & CONF_REPEATABLE_MASK) &&
		    entry->configParamList->numberOfNodes) {
			param = entry->configParamList->firstNode->data;
			FATAL("config parameter \"%s\" is first defined on line "
			     "%i and redefined on line %i\n", array[0],
			     param->line, count);
		}

		if (entry->mask & CONF_BLOCK_MASK) {
			if (0 != strcmp(array[1], CONF_BLOCK_BEGIN)) {
				FATAL("improperly formatted config file at "
				      "line %i: %s\n", count, string);
			}
			param = readConfigBlock(fp, &count, string);
		} else
			param = newConfigParam(array[1], count);

		insertInListWithoutKey(entry->configParamList, param);
	}
	fclose(fp);
}

ConfigParam *getNextConfigParam(char *name, ConfigParam * last)
{
	void *voidPtr;
	ConfigEntry *entry;
	ListNode *node;
	ConfigParam *param;

	if (!findInList(configEntriesList, name, &voidPtr))
		return NULL;

	entry = voidPtr;

	node = entry->configParamList->firstNode;

	if (last) {
		while (node != NULL) {
			param = node->data;
			node = node->nextNode;
			if (param == last)
				break;
		}
	}

	if (node == NULL)
		return NULL;

	param = node->data;

	return param;
}

char *getConfigParamValue(char *name)
{
	ConfigParam *param = getConfigParam(name);

	if (!param)
		return NULL;

	return param->value;
}

int getBoolConfigParam(char *name)
{
	ConfigParam *param;
	
	param = getConfigParam(name);
	if (!param) return -1;

	if (strcmp("yes", param->value) == 0) return 1;
	else if (strcmp("no", param->value) == 0) return 0;

	ERROR("%s is not \"yes\" or \"no\" on line %i\n", name, param->line);

	return -2;
}

BlockParam *getBlockParam(ConfigParam * param, char *name)
{
	BlockParam *ret = NULL;
	int i;

	for (i = 0; i < param->numberOfBlockParams; i++) {
		if (0 == strcmp(name, param->blockParams[i].name)) {
			if (ret) {
				ERROR("\"%s\" first defined on line %i, and "
				      "redefined on line %i\n", name,
				      ret->line, param->blockParams[i].line);
			}
			ret = param->blockParams + i;
		}
	}

	return ret;
}

ConfigParam *parseConfigFilePath(char *name, int force)
{
	ConfigParam *param = getConfigParam(name);
	char *path;

	if (!param && force)
		FATAL("config parameter \"%s\" not found\n", name);

	if (!param)
		return NULL;

	path = param->value;

	if (path[0] != '/' && path[0] != '~') {
		FATAL("\"%s\" is not an absolute path at line %i\n",
		      param->value, param->line);
	}
	/* Parse ~ in path */
	else if (path[0] == '~') {
		struct passwd *pwd = NULL;
		char *newPath;
		int pos = 1;
		if (path[1] == '/' || path[1] == '\0') {
			ConfigParam *userParam = getConfigParam(CONF_USER);

			if (userParam) {
				pwd = getpwnam(userParam->value);
				if (!pwd) {
					FATAL("no such user %s at line %i\n",
					      userParam->value,
					      userParam->line);
				}
			} else {
				uid_t uid = geteuid();
				if ((pwd = getpwuid(uid)) == NULL) {
					FATAL("problems getting passwd entry "
					      "for current user\n");
				}
			}
		} else {
			int foundSlash = 0;
			char *ch = path + 1;
			for (; *ch != '\0' && *ch != '/'; ch++) ;
			if (*ch == '/')
				foundSlash = 1;
			*ch = '\0';
			pos += ch - path - 1;
			if ((pwd = getpwnam(path + 1)) == NULL) {
				FATAL("user \"%s\" not found at line %i\n",
				      path + 1, param->line);
			}
			if (foundSlash)
				*ch = '/';
		}
		newPath = xmalloc(strlen(pwd->pw_dir) + strlen(path + pos) + 1);
		strcpy(newPath, pwd->pw_dir);
		strcat(newPath, path + pos);
		free(param->value);
		param->value = newPath;
	}

	return param;
}
