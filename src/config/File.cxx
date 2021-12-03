/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "File.hxx"
#include "Data.hxx"
#include "Param.hxx"
#include "Block.hxx"
#include "Templates.hxx"
#include "system/Error.hxx"
#include "util/Tokenizer.hxx"
#include "util/StringStrip.hxx"
#include "util/StringAPI.hxx"
#include "util/Domain.hxx"
#include "util/RuntimeError.hxx"
#include "fs/FileSystem.hxx"
#include "fs/List.hxx"
#include "fs/Path.hxx"
#include "io/FileReader.hxx"
#include "io/BufferedReader.hxx"
#include "Log.hxx"

#include <cassert>

static constexpr char CONF_COMMENT = '#';

static constexpr Domain config_file_domain("config_file");

/**
 * Read a string value as the last token of a line.  Throws on error.
 */
static auto
ExpectValueAndEnd(Tokenizer &tokenizer)
{
	auto value = tokenizer.NextString();
	if (!value)
		throw std::runtime_error("Value missing");

	if (!tokenizer.IsEnd() && tokenizer.CurrentChar() != CONF_COMMENT)
		throw std::runtime_error("Unknown tokens after value");

	return value;
}

static void
config_read_name_value(ConfigBlock &block, char *input, unsigned line)
{
	Tokenizer tokenizer(input);

	const char *name = tokenizer.NextWord();
	assert(name != nullptr);

	auto value = ExpectValueAndEnd(tokenizer);

	const BlockParam *bp = block.GetBlockParam(name);
	if (bp != nullptr)
		throw FormatRuntimeError("\"%s\" is duplicate, first defined on line %i",
					 name, bp->line);

	block.AddBlockParam(name, value, line);
}

static ConfigBlock
config_read_block(BufferedReader &reader)
{
	ConfigBlock block(reader.GetLineNumber());

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

			return block;
		}

		/* parse name and value */

		config_read_name_value(block, line,
				       reader.GetLineNumber());
	}
}

static void
ReadConfigBlock(ConfigData &config_data, BufferedReader &reader,
		const char *name, ConfigBlockOption o,
		Tokenizer &tokenizer)
{
	const auto i = unsigned(o);
	const ConfigTemplate &option = config_block_templates[i];

	if (option.deprecated)
		FmtWarning(config_file_domain,
			   "config parameter \"{}\" on line {} is deprecated",
			   name, reader.GetLineNumber());

	if (!option.repeatable)
		if (const auto *block = config_data.GetBlock(o))
			throw FormatRuntimeError("config parameter \"%s\" is first defined "
						 "on line %d and redefined on line %u\n",
						 name, block->line,
						 reader.GetLineNumber());

	/* now parse the block or the value */

	if (tokenizer.CurrentChar() != '{')
		throw std::runtime_error("'{' expected");

	char *line = StripLeft(tokenizer.Rest() + 1);
	if (*line != 0 && *line != CONF_COMMENT)
		throw std::runtime_error("Unknown tokens after '{'");

	config_data.AddBlock(o, config_read_block(reader));
}

static void
ReadConfigParam(ConfigData &config_data, BufferedReader &reader,
		const char *name, ConfigOption o,
		Tokenizer &tokenizer)
{
	const auto i = unsigned(o);
	const ConfigTemplate &option = config_param_templates[i];

	if (option.deprecated)
		FmtWarning(config_file_domain,
			   "config parameter \"{}\" on line {} is deprecated",
			   name, reader.GetLineNumber());

	if (!option.repeatable)
		/* if the option is not repeatable, override the old
		   value by removing it first */
		config_data.GetParamList(o).clear();

	/* now parse the block or the value */

	config_data.AddParam(o, ConfigParam(ExpectValueAndEnd(tokenizer),
					    reader.GetLineNumber()));
}

/**
 * @param directory the directory used to resolve relative paths
 */
static void
ReadConfigFile(ConfigData &config_data, BufferedReader &reader, Path directory)
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

		if (StringIsEqual(name, "include")) {
			// TODO: detect recursion
			// TODO: Config{Block,Param} have only line number but no file name
			const auto pattern = AllocatedPath::Apply(directory,
								  AllocatedPath::FromUTF8Throw(ExpectValueAndEnd(tokenizer)));
			for (const auto &path : ListWildcard(pattern))
				ReadConfigFile(config_data, path);
			continue;
		}

		if (StringIsEqual(name, "include_optional")) {
			const auto pattern = AllocatedPath::Apply(directory,
								  AllocatedPath::FromUTF8Throw(ExpectValueAndEnd(tokenizer)));

			std::forward_list<AllocatedPath> l;
			try {
				l = ListWildcard(pattern);
			} catch (const std::system_error &e) {
				/* ignore "file not found */
				if (!IsFileNotFound(e) && !IsPathNotFound(e))
					throw;
			}

			for (const auto &path : l)
				if (PathExists(path))
					ReadConfigFile(config_data, path);
			continue;
		}

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
			throw FormatRuntimeError("unrecognized parameter: %s\n",
						 name);
		}
	}
}

void
ReadConfigFile(ConfigData &config_data, Path path)
{
	assert(!path.IsNull());
	const std::string path_utf8 = path.ToUTF8();

	FmtDebug(config_file_domain, "loading file {}", path_utf8);

	FileReader file(path);

	BufferedReader reader(file);

	try {
		ReadConfigFile(config_data, reader, path.GetDirectoryName());
	} catch (...) {
		std::throw_with_nested(FormatRuntimeError("Error in %s line %u",
							  path_utf8.c_str(),
							  reader.GetLineNumber()));
	}
}
