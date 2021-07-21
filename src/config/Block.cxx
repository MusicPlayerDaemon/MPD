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

#include "Block.hxx"
#include "Parser.hxx"
#include "Path.hxx"
#include "fs/AllocatedPath.hxx"
#include "util/RuntimeError.hxx"

#include <stdlib.h>

void
BlockParam::ThrowWithNested() const
{
	std::throw_with_nested(FormatRuntimeError("Error in setting \"%s\" on line %i",
						  name.c_str(), line));
}

int
BlockParam::GetIntValue() const
{
	const char *const s = value.c_str();
	char *endptr;
	long value2 = strtol(s, &endptr, 0);
	if (endptr == s || *endptr != 0)
		throw FormatRuntimeError("Not a valid number in line %i", line);

	return value2;
}

unsigned
BlockParam::GetUnsignedValue() const
{
	return With(ParseUnsigned);
}

unsigned
BlockParam::GetPositiveValue() const
{
	return With(ParsePositive);
}

bool
BlockParam::GetBoolValue() const
{
	return With(ParseBool);
}

const BlockParam *
ConfigBlock::GetBlockParam(const char *name) const noexcept
{
	for (const auto &i : block_params) {
		if (i.name == name) {
			i.used = true;
			return &i;
		}
	}

	return nullptr;
}

const char *
ConfigBlock::GetBlockValue(const char *name,
			   const char *default_value) const noexcept
{
	const BlockParam *bp = GetBlockParam(name);
	if (bp == nullptr)
		return default_value;

	return bp->value.c_str();
}

AllocatedPath
ConfigBlock::GetPath(const char *name, const char *default_value) const
{
	const char *s;

	const BlockParam *bp = GetBlockParam(name);
	if (bp != nullptr) {
		s = bp->value.c_str();
	} else {
		if (default_value == nullptr)
			return nullptr;

		s = default_value;
	}

	return ParsePath(s);
}

int
ConfigBlock::GetBlockValue(const char *name, int default_value) const
{
	const BlockParam *bp = GetBlockParam(name);
	if (bp == nullptr)
		return default_value;

	return bp->GetIntValue();
}

unsigned
ConfigBlock::GetBlockValue(const char *name, unsigned default_value) const
{
	const BlockParam *bp = GetBlockParam(name);
	if (bp == nullptr)
		return default_value;

	return bp->GetUnsignedValue();
}

unsigned
ConfigBlock::GetPositiveValue(const char *name, unsigned default_value) const
{
	const auto *param = GetBlockParam(name);
	if (param == nullptr)
		return default_value;

	return param->GetPositiveValue();
}

bool
ConfigBlock::GetBlockValue(const char *name, bool default_value) const
{
	const BlockParam *bp = GetBlockParam(name);
	if (bp == nullptr)
		return default_value;

	return bp->GetBoolValue();
}
