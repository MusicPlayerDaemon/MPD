// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Block.hxx"
#include "Parser.hxx"
#include "Path.hxx"
#include "fs/AllocatedPath.hxx"
#include "lib/fmt/RuntimeError.hxx"

#include <stdlib.h>

void
BlockParam::ThrowWithNested() const
{
	std::throw_with_nested(FmtRuntimeError("Error in setting {:?} on line {}",
					       name, line));
}

int
BlockParam::GetIntValue() const
{
	const char *const s = value.c_str();
	char *endptr;
	long value2 = strtol(s, &endptr, 0);
	if (endptr == s || *endptr != 0)
		throw FmtRuntimeError("Not a valid number in line {}", line);

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

std::chrono::steady_clock::duration
BlockParam::GetDuration(std::chrono::steady_clock::duration min_value) const
{
	return With([min_value](const char *s){
		auto duration = ParseDuration(s);
		if (duration < min_value)
			throw std::invalid_argument{"Value is too small"};

		return duration;
	});
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

std::chrono::steady_clock::duration
ConfigBlock::GetDuration(const char *name,
			 std::chrono::steady_clock::duration min_value,
			 std::chrono::steady_clock::duration default_value) const
{
	const BlockParam *bp = GetBlockParam(name);
	if (bp == nullptr)
		return default_value;

	return bp->GetDuration(min_value);
}

void
ConfigBlock::ThrowWithNested() const
{
	std::throw_with_nested(FmtRuntimeError("Error in block on line {}",
					       line));
}
