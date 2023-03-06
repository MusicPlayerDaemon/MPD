// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Error.hxx"

#include <spa/utils/result.h>

namespace PipeWire {

ErrorCategory error_category;

std::string
ErrorCategory::message(int condition) const
{
	return spa_strerror(condition);
}

} // namespace PipeWire
