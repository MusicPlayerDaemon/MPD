// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <string>

void
trimstring(std::string &s, const char *ws = " \t\n") noexcept;

std::string
path_getfather(std::string_view s) noexcept;
