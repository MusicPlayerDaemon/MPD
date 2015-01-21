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
#include "ConfigData.hxx"
#include "ConfigPath.hxx"
#include "util/Error.hxx"
#include "fs/AllocatedPath.hxx"

#include <assert.h>
#include <stdlib.h>

config_param::config_param(const char *_value, int _line)
	:next(nullptr), value(_value), line(_line), used(false) {}

config_param::~config_param()
{
	delete next;
}

const BlockParam *
config_param::GetBlockParam(const char *name) const
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
config_param::GetBlockValue(const char *name, const char *default_value) const
{
	const BlockParam *bp = GetBlockParam(name);
	if (bp == nullptr)
		return default_value;

	return bp->value.c_str();
}

AllocatedPath
config_param::GetBlockPath(const char *name, const char *default_value,
			   Error &error) const
{
	assert(!error.IsDefined());

	int line2 = line;
	const char *s;

	const BlockParam *bp = GetBlockParam(name);
	if (bp != nullptr) {
		line2 = bp->line;
		s = bp->value.c_str();
	} else {
		if (default_value == nullptr)
			return AllocatedPath::Null();

		s = default_value;
	}

	AllocatedPath path = ParsePath(s, error);
	if (gcc_unlikely(path.IsNull()))
		error.FormatPrefix("Invalid path in \"%s\" at line %i: ",
				   name, line2);

	return path;
}

AllocatedPath
config_param::GetBlockPath(const char *name, Error &error) const
{
	return GetBlockPath(name, nullptr, error);
}

int
config_param::GetBlockValue(const char *name, int default_value) const
{
	const BlockParam *bp = GetBlockParam(name);
	if (bp == nullptr)
		return default_value;

	return bp->GetIntValue();
}

unsigned
config_param::GetBlockValue(const char *name, unsigned default_value) const
{
	const BlockParam *bp = GetBlockParam(name);
	if (bp == nullptr)
		return default_value;

	return bp->GetUnsignedValue();
}

gcc_pure
bool
config_param::GetBlockValue(const char *name, bool default_value) const
{
	const BlockParam *bp = GetBlockParam(name);
	if (bp == nullptr)
		return default_value;

	return bp->GetBoolValue();
}
