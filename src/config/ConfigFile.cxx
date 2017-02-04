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
#include "ConfigFile.hxx"
#include "Data.hxx"
#include "Param.hxx"
#include "Block.hxx"
#include "ConfigTemplates.hxx"
#include "util/Tokenizer.hxx"
#include "util/StringUtil.hxx"
#include "util/Domain.hxx"
#include "util/RuntimeError.hxx"
#include "fs/Path.hxx"
#include "fs/io/FileReader.hxx"
#include "fs/io/BufferedReader.hxx"
#include "Log.hxx"

#include <memory>

#include <assert.h>

static constexpr char CONF_COMMENT = '#';

static constexpr Domain config_file_domain("config_file");

static void
config_read_name_value(ConfigBlock &block, char *input, unsigned line)
{
	Tokenizer tokenizer(input);

	const char *name = tokenizer.NextWord();
	assert(name != nullptr);

	const char *value = tokenizer.NextString();
	if (value == nullptr)
		throw std::runtime_error("Value missing");

	if (!tokenizer.IsEnd() && tokenizer.CurrentChar() != CONF_COMMENT)
		throw std::runtime_error("Unknown tokens after value");

	const BlockParam *bp = block.GetBlockParam(name);
	if (bp != nullptr)
		throw FormatRuntimeError("\"%s\" is duplicate, first defined on line %i",
					 name, bp->line);

	block.AddBlockParam(name, value, line);
}

static ConfigBlock *
config_read_block(BufferedReader &reader)
try {
	std::unique_ptr<ConfigBlock> block(new ConfigBlock(reader.GetLineNumber()));

	while (true) {
		char *line = reader.ReadLine();
		if (line == nullptr)
			throw std::runtime_error("Expected '}' before end-of-file");

		line = StripLeft(line);
		if (*line == 0 || *line == CONF_COMMENT)
			continue;

		if (*line == '}') {
			/* end of this block; return from the function
			   (and from this "while" loop) */

			line = StripLeft(line + 1);
			if (*line != 0 && *line != CONF_COMMENT)
				throw std::runtime_error("Unknown tokens after '}'");

			return block.release();
		}

		/* parse name and value */

		config_read_name_value(*block, line,
				       reader.GetLineNumber());
	}
} catch (...) {
	std::throw_with_nested(FormatRuntimeError("Error in line %u", reader.GetLineNumber()));
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

static void
ReadConfigBlock(ConfigData &config_data, BufferedReader &reader,
		const char *name, ConfigBlockOption o,
		Tokenizer &tokenizer)
{
	const unsigned i = unsigned(o);
	const ConfigTemplate &option = config_block_templates[i];
	ConfigBlock *&head = config_data.blocks[i];

	if (head != nullptr && !option.repeatable) {
		ConfigBlock *block = head;
		throw FormatRuntimeError("config parameter \"%s\" is first defined "
					 "on line %d and redefined on line %u\n",
					 name, block->line,
					 reader.GetLineNumber());
	}

	/* now parse the block or the value */

	if (tokenizer.CurrentChar() != '{')
		throw FormatRuntimeError("line %u: '{' expected",
					 reader.GetLineNumber());

	char *line = StripLeft(tokenizer.Rest() + 1);
	if (*line != 0 && *line != CONF_COMMENT)
		throw FormatRuntimeError("line %u: Unknown tokens after '{'",
					 reader.GetLineNumber());

	auto *param = config_read_block(reader);
	assert(param != nullptr);
	Append(head, param);
}

gcc_nonnull_all
static void
Append(ConfigParam *&head, ConfigParam *p)
{
	assert(p->next == nullptr);

	auto **i = &head;
	while (*i != nullptr)
		i = &(*i)->next;

	*i = p;
}

static void
ReadConfigParam(ConfigData &config_data, BufferedReader &reader,
		const char *name, ConfigOption o,
		Tokenizer &tokenizer)
{
	const unsigned i = unsigned(o);
	const ConfigTemplate &option = config_param_templates[i];
	auto *&head = config_data.params[i];

	if (head != nullptr && !option.repeatable) {
		auto *param = head;
		throw FormatRuntimeError("config parameter \"%s\" is first defined "
					 "on line %d and redefined on line %u\n",
					 name, param->line,
					 reader.GetLineNumber());
	}

	/* now parse the block or the value */

	const char *value = tokenizer.NextString();
	if (value == nullptr)
		throw FormatRuntimeError("line %u: Value missing",
					 reader.GetLineNumber());

	if (!tokenizer.IsEnd() && tokenizer.CurrentChar() != CONF_COMMENT)
		throw FormatRuntimeError("line %u: Unknown tokens after value",
					 reader.GetLineNumber());

	auto *param = new ConfigParam(value, reader.GetLineNumber());
	Append(head, param);
}

static void
ReadConfigFile(ConfigData &config_data, BufferedReader &reader)
{
	while (true) {
		char *line = reader.ReadLine();
		if (line == nullptr)
			return;

		line = StripLeft(line);
		if (*line == 0 || *line == CONF_COMMENT)
			continue;

		/* the first token in each line is the name, followed
		   by either the value or '{' */

		Tokenizer tokenizer(line);
		const char *name = tokenizer.NextWord();
		assert(name != nullptr);

		/* get the definition of that option, and check the
		   "repeatable" flag */

		const ConfigOption o = ParseConfigOptionName(name);
		ConfigBlockOption bo;
		if (o != ConfigOption::MAX) {
			ReadConfigParam(config_data, reader, name, o,
					tokenizer);
		} else if ((bo = ParseConfigBlockOptionName(name)) != ConfigBlockOption::MAX) {
			ReadConfigBlock(config_data, reader, name, bo,
					tokenizer);
		} else {
			throw FormatRuntimeError("unrecognized parameter in config file at "
						 "line %u: %s\n",
						 reader.GetLineNumber(), name);
		}
	}
}

void
ReadConfigFile(ConfigData &config_data, Path path)
{
	assert(!path.IsNull());
	const std::string path_utf8 = path.ToUTF8();

	FormatDebug(config_file_domain, "loading file %s", path_utf8.c_str());

	FileReader file(path);

	BufferedReader reader(file);
	ReadConfigFile(config_data, reader);
}
