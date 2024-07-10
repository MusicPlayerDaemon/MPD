// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Data.hxx"
#include "Parser.hxx"
#include "fs/AllocatedPath.hxx"
#include "lib/fmt/RuntimeError.hxx"
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
[[gnu::pure]]
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
ConfigData::GetDuration(ConfigOption option,
			std::chrono::steady_clock::duration min_value,
			std::chrono::steady_clock::duration default_value) const
{
	return With(option, [min_value, default_value](const char *s){
		if (s == nullptr)
			return default_value;

		auto value = ParseDuration(s);
		if (value < min_value)
			throw std::runtime_error{"Value is too small"};

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
			throw FmtRuntimeError("block without {:?} in line {}",
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
