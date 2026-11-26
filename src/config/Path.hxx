// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include <string_view>

class AllocatedPath;

/**
 * Throws #std::runtime_error on error.
 */
AllocatedPath
ParsePath(std::string_view path);
