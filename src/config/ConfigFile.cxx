/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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
#include "Data.hxx"
#include "Param.hxx"
#include "Block.hxx"
#include "ConfigTemplates.hxx"
#include "util/Tokenizer.hxx"
#include "util/StringUtil.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "fs/Path.hxx"
#include "fs/io/FileReader.hxx"
#include "fs/io/BufferedReader.hxx"
#include "Log.hxx"

#include <assert.h>

static constexpr char CONF_COMMENT = '#';

static constexpr Domain config_file_domain("config_file");

static bool
config_read_name_value(ConfigBlock &block, char *input, unsigned line,
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

	const BlockParam *bp = block.GetBlockParam(name);
	if (bp != nullptr) {
		error.Format(config_file_domain,
			     "\"%s\" is duplicate, first defined on line %i",
			     name, bp->line);
		return false;
	}

	block.AddBlockParam(name, value, line);
	return true;
}

static ConfigBlock *
config_read_block(BufferedReader &reader, Error &error)
{
	auto *ret = new ConfigBlock(reader.GetLineNumber());

	while (true) {
		char *line = reader.ReadLine();
		if (line == nullptr) {
			delete ret;

			if (reader.Check(error))
				error.Set(config_file_domain,
					  "Expected '}' before end-of-file");
			return nullptr;
		}

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
					     "line %y: Unknown tokens after '}'",
					     reader.GetLineNumber());
				return nullptr;
			}

			return ret;
		}

		/* parse name and value */

		if (!config_read_name_value(*ret, line, reader.GetLineNumber(),
					    error)) {
			assert(*line != 0);
			delete ret;
			error.FormatPrefix("line %u: ", reader.GetLineNumber());
			return nullptr;
		}
	}
}

gcc_nonnull_all
static void
Append(ConfigBlock *&head, ConfigBlock *p)
{
	assert(p->next == nullptr);

	auto **i = &head;
	while (*i != nullptr)
		i = &(*i)->next;

	*i = p;
}

static bool
ReadConfigBlock(ConfigData &config_data, BufferedReader &reader,
		const char *name, ConfigOption o,
		Tokenizer &tokenizer,
		Error &error)
{
	const unsigned i = unsigned(o);
	const ConfigTemplate &option = config_block_templates[i];
	ConfigBlock *&head = config_data.blocks[i];

	if (head != nullptr && !option.repeatable) {
		ConfigBlock *block = head;
		error.Format(config_file_domain,
			     "config parameter \"%s\" is first defined "
			     "on line %d and redefined on line %u\n",
			     name, block->line,
			     reader.GetLineNumber());
		return false;
	}

	/* now parse the block or the value */

	if (tokenizer.CurrentChar() != '{') {
		error.Format(config_file_domain,
			     "line %u: '{' expected",
			     reader.GetLineNumber());
		return false;
	}

	char *line = StripLeft(tokenizer.Rest() + 1);
	if (*line != 0 && *line != CONF_COMMENT) {
		error.Format(config_file_domain,
			     "line %u: Unknown tokens after '{'",
			     reader.GetLineNumber());
		return false;
	}

	auto *param = config_read_block(reader, error);
	if (param == nullptr)
		return false;

	Append(head, param);
	return true;
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
ReadConfigParam(ConfigData &config_data, BufferedReader &reader,
		const char *name, ConfigOption o,
		Tokenizer &tokenizer,
		Error &error)
{
	const unsigned i = unsigned(o);
	const ConfigTemplate &option = config_param_templates[i];
	config_param *&head = config_data.params[i];

	if (head != nullptr && !option.repeatable) {
		struct config_param *param = head;
		error.Format(config_file_domain,
			     "config parameter \"%s\" is first defined "
			     "on line %d and redefined on line %u\n",
			     name, param->line,
			     reader.GetLineNumber());
		return false;
	}

	/* now parse the block or the value */

	const char *value = tokenizer.NextString(error);
	if (value == nullptr) {
		if (tokenizer.IsEnd())
			error.Format(config_file_domain,
				     "line %u: Value missing",
				     reader.GetLineNumber());
		else
			error.FormatPrefix("line %u: ",
					   reader.GetLineNumber());

		return false;
	}

	if (!tokenizer.IsEnd() &&
	    tokenizer.CurrentChar() != CONF_COMMENT) {
		error.Format(config_file_domain,
			     "line %u: Unknown tokens after value",
			     reader.GetLineNumber());
		return false;
	}

	auto *param = new config_param(value, reader.GetLineNumber());
	Append(head, param);
	return true;
}

static bool
ReadConfigFile(ConfigData &config_data, BufferedReader &reader, Error &error)
{
	while (true) {
		char *line = reader.ReadLine();
		if (line == nullptr)
			return true;

		line = StripLeft(line);
		if (*line == 0 || *line == CONF_COMMENT)
			continue;

		/* the first token in each line is the name, followed
		   by either the value or '{' */

		Tokenizer tokenizer(line);
		const char *name = tokenizer.NextWord(error);
		if (name == nullptr) {
			assert(!tokenizer.IsEnd());
			error.FormatPrefix("line %u: ", reader.GetLineNumber());
			return false;
		}

		/* get the definition of that option, and check the
		   "repeatable" flag */

		const ConfigOption o = ParseConfigOptionName(name);
		ConfigBlockOption bo;
		if (o != ConfigOption::MAX) {
			if (!ReadConfigParam(config_data, reader, name, o,
					     tokenizer, error))
				return false;
		} else if ((bo = ParseConfigBlockOptionName(name)) != ConfigBlockOption::MAX) {
			if (!ReadConfigBlock(config_data, reader, name, o,
					     tokenizer, error))
				return false;
		} else {
			error.Format(config_file_domain,
				     "unrecognized parameter in config file at "
				     "line %u: %s\n",
				     reader.GetLineNumber(), name);
			return false;
		}
	}
}

bool
ReadConfigFile(ConfigData &config_data, Path path, Error &error)
{
	assert(!path.IsNull());
	const std::string path_utf8 = path.ToUTF8();

	FormatDebug(config_file_domain, "loading file %s", path_utf8.c_str());

	FileReader file(path, error);
	if (!file.IsDefined())
		return false;

	BufferedReader reader(file);
	return ReadConfigFile(config_data, reader, error) &&
		reader.Check(error);
}
