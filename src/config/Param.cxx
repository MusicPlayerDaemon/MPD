// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Param.hxx"
#include "Path.hxx"
#include "fs/AllocatedPath.hxx"
#include "lib/fmt/RuntimeError.hxx"

#include <stdexcept>

void
ConfigParam::ThrowWithNested() const
{
	std::throw_with_nested(FmtRuntimeError("Error on line {}", line));
}

AllocatedPath
ConfigParam::GetPath() const
{
	try {
		return ParsePath(value.c_str());
	} catch (...) {
		ThrowWithNested();
	}

}
