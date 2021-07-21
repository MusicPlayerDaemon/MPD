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

#include "Data.hxx"
#include "Parser.hxx"
#include "fs/AllocatedPath.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringAPI.hxx"

#include <stdlib.h>

void
ConfigData::Clear()
{
	for (auto &i : params)
		i.clear();

	for (auto &i : blocks)
		i.clear();
}

template<typename T>
gcc_pure
static auto
FindLast(const std::forward_list<T> &list)
{
	auto i = list.before_begin();
	while (std::next(i) != list.end())
		++i;
	return i;
}

template<typename T>
static auto
Append(std::forward_list<T> &list, T &&src)
{
	return list.emplace_after(FindLast(list), std::forward<T>(src));
}

void
ConfigData::AddParam(ConfigOption option,
		     ConfigParam &&param) noexcept
{
	Append(GetParamList(option), std::move(param));
}

const char *
ConfigData::GetString(ConfigOption option,
		      const char *default_value) const noexcept
{
	const auto *param = GetParam(option);
	if (param == nullptr)
		return default_value;

	return param->value.c_str();
}

AllocatedPath
ConfigData::GetPath(ConfigOption option) const
{
	const auto *param = GetParam(option);
	if (param == nullptr)
		return nullptr;

	return param->GetPath();
}

unsigned
ConfigData::GetUnsigned(ConfigOption option, unsigned default_value) const
{
	return With(option, [default_value](const char *s){
		return s != nullptr
			? ParseUnsigned(s)
			: default_value;
	});
}

unsigned
ConfigData::GetPositive(ConfigOption option, unsigned default_value) const
{
	return With(option, [default_value](const char *s){
		return s != nullptr
			? ParsePositive(s)
			: default_value;
	});
}

std::chrono::steady_clock::duration
ConfigData::GetUnsigned(ConfigOption option,
			std::chrono::steady_clock::duration default_value) const
{
	return With(option, [default_value](const char *s){
		if (s == nullptr)
			return default_value;

		auto value = ParseDuration(s);
		if (value < std::chrono::steady_clock::duration{})
			throw std::runtime_error("Value must not be negative");

		return value;
	});
}

std::chrono::steady_clock::duration
ConfigData::GetPositive(ConfigOption option,
			std::chrono::steady_clock::duration default_value) const
{
	return With(option, [default_value](const char *s){
		if (s == nullptr)
			return default_value;

		auto value = ParseDuration(s);
		if (value <= std::chrono::steady_clock::duration{})
			throw std::runtime_error("Value must be positive");

		return value;
	});
}

bool
ConfigData::GetBool(ConfigOption option, bool default_value) const
{
	return With(option, [default_value](const char *s){
		return s != nullptr
			? ParseBool(s)
			: default_value;
	});
}

ConfigBlock &
ConfigData::AddBlock(ConfigBlockOption option,
		     ConfigBlock &&block) noexcept
{
	return *Append(GetBlockList(option), std::move(block));
}

const ConfigBlock *
ConfigData::FindBlock(ConfigBlockOption option,
		      const char *key, const char *value) const
{
	for (const auto &block : GetBlockList(option)) {
		const char *value2 = block.GetBlockValue(key);
		if (value2 == nullptr)
			throw FormatRuntimeError("block without '%s' in line %d",
						 key, block.line);

		if (StringIsEqual(value2, value))
			return &block;
	}

	return nullptr;
}

ConfigBlock &
ConfigData::MakeBlock(ConfigBlockOption option,
		      const char *key, const char *value)
{
	auto *block = const_cast<ConfigBlock *>(FindBlock(option, key, value));
	if (block == nullptr) {
		ConfigBlock new_block;
		new_block.AddBlockParam(key, value);
		block = &AddBlock(option, std::move(new_block));
	}

	return *block;
}
