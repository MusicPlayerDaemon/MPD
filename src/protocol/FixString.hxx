// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <string_view>

template<typename T> class AllocatedArray;

AllocatedArray<char>
FixStringUTF8(std::string_view p) noexcept;
