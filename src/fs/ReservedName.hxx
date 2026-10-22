// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <string_view>

/**
 * Is this a file name which Windows interprets as a DOS device (e.g.
 * "CON", "NUL" or "COM1"), even with an extension (e.g. "COM1.m3u")
 * and in any directory?
 */
[[gnu::pure]]
bool
IsWindowsReservedFilename(std::string_view name) noexcept;
