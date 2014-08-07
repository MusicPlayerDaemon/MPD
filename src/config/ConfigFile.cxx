/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "ConfigData.hxx"
#include "ConfigTemplates.hxx"
#include "util/Tokenizer.hxx"
#include "util/StringUtil.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "fs/Limits.hxx"
#include "fs/Path.hxx"
#include "fs/FileSystem.hxx"
#include "Log.hxx"

#include <assert.h>
#include <stdio.h>

#define MAX_STRING_SIZE	MPD_PATH_MAX+80

#define CONF_COMMENT		'#'

static constexpr Domain config_file_domain("config_file");

static bool
config_read_name_value(struct config_param *param, char *input, unsigned line,
		       Error &error)
{
	Tokenizer tokenizer(input);

	const char *name = tokenizer.NextWord(error);
	if (name == nullptr) {
		assert(!tokenizer.IsEnd());
		return false;
	}

	const char *value = tokenizer.NextString(error);
	if (value == nullptr) {
		if (tokenizer.IsEnd()) {
			error.Set(config_file_domain, "Value missing");
		} else {
			assert(error.IsDefined());
		}

		return false;
	}

	if (!tokenizer.IsEnd() && tokenizer.CurrentChar() != CONF_COMMENT) {
		error.Set(config_file_domain, "Unknown tokens after value");
		return false;
	}

	const struct block_param *bp = param->GetBlockParam(name);
	if (bp != nullptr) {
		error.Format(config_file_domain,
			     "\"%s\" is duplicate, first defined on line %i",
			     name, bp->line);
		return false;
	}

	param->AddBlockParam(name, value, line);
	return true;
}

static struct config_param *
config_read_block(FILE *fp, int *count, char *string, Error &error)
{
	struct config_param *ret = new config_param(*count);

	while (true) {
		char *line;

		line = fgets(string, MAX_STRING_SIZE, fp);
		if (line == nullptr) {
			delete ret;
			error.Set(config_file_domain,
				  "Expected '}' before end-of-file");
			return nullptr;
		}

		(*count)++;
		line = StripLeft(line);
		if (*line == 0 || *line == CONF_COMMENT)
			continue;

		if (*line == '}') {
			/* end of this block; return from the function
			   (and from this "while" loop) */

			line = StripLeft(line + 1);
			if (*line != 0 && *line != CONF_COMMENT) {
				delete ret;
				error.Format(config_file_domain,
					     "line %i: Unknown tokens after '}'",
					     *count);
				return nullptr;
			}

			return ret;
		}

		/* parse name and value */

		if (!config_read_name_value(ret, line, *count, error)) {
			assert(*line != 0);
			delete ret;
			error.FormatPrefix("line %i: ", *count);
			return nullptr;
		}
	}
}

gcc_nonnull_all
static void
Append(config_param *&head, config_param *p)
{
	assert(p->next == nullptr);

	config_param **i = &head;
	while (*i != nullptr)
		i = &(*i)->next;

	*i = p;
}

static bool
ReadConfigFile(ConfigData &config_data, FILE *fp, Error &error)
{
	assert(fp != nullptr);

	char string[MAX_STRING_SIZE + 1];
	int count = 0;
	struct config_param *param;

	while (fgets(string, MAX_STRING_SIZE, fp)) {
		char *line;
		const char *name, *value;

		count++;

		line = StripLeft(string);
		if (*line == 0 || *line == CONF_COMMENT)
			continue;

		/* the first token in each line is the name, followed
		   by either the value or '{' */

		Tokenizer tokenizer(line);
		name = tokenizer.NextWord(error);
		if (name == nullptr) {
			assert(!tokenizer.IsEnd());
			error.FormatPrefix("line %i: ", count);
			return false;
		}

		/* get the definition of that option, and check the
		   "repeatable" flag */

		const ConfigOption o = ParseConfigOptionName(name);
		if (o == CONF_MAX) {
			error.Format(config_file_domain,
				     "unrecognized parameter in config file at "
				     "line %i: %s\n", count, name);
			return false;
		}

		const unsigned i = unsigned(o);
		const ConfigTemplate &option = config_templates[i];
		config_param *&head = config_data.params[i];

		if (head != nullptr && !option.repeatable) {
			param = head;
			error.Format(config_file_domain,
				     "config parameter \"%s\" is first defined "
				     "on line %i and redefined on line %i\n",
				     name, param->line, count);
			return false;
		}

		/* now parse the block or the value */

		if (option.block) {
			/* it's a block, call config_read_block() */

			if (tokenizer.CurrentChar() != '{') {
				error.Format(config_file_domain,
					     "line %i: '{' expected", count);
				return false;
			}

			line = StripLeft(tokenizer.Rest() + 1);
			if (*line != 0 && *line != CONF_COMMENT) {
				error.Format(config_file_domain,
					     "line %i: Unknown tokens after '{'",
					     count);
				return false;
			}

			param = config_read_block(fp, &count, string, error);
			if (param == nullptr) {
				return false;
			}
		} else {
			/* a string value */

			value = tokenizer.NextString(error);
			if (value == nullptr) {
				if (tokenizer.IsEnd())
					error.Format(config_file_domain,
						     "line %i: Value missing",
						     count);
				else
					error.FormatPrefix("line %i: ", count);

				return false;
			}

			if (!tokenizer.IsEnd() &&
			    tokenizer.CurrentChar() != CONF_COMMENT) {
				error.Format(config_file_domain,
					     "line %i: Unknown tokens after value",
					     count);
				return false;
			}

			param = new config_param(value, count);
		}

		Append(head, param);
	}

	return true;
}

bool
ReadConfigFile(ConfigData &config_data, Path path, Error &error)
{
	assert(!path.IsNull());
	const std::string path_utf8 = path.ToUTF8();

	FormatDebug(config_file_domain, "loading file %s", path_utf8.c_str());

	FILE *fp = FOpen(path, FOpenMode::ReadText);
	if (fp == nullptr) {
		error.FormatErrno("Failed to open %s", path_utf8.c_str());
		return false;
	}

	bool result = ReadConfigFile(config_data, fp, error);
	fclose(fp);
	return result;
}
