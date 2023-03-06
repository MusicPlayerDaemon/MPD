// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <string_view>

class MixRampInfo;

bool
ParseMixRampTag(MixRampInfo &info,
		const char *name, const char *value) noexcept;

bool
ParseMixRampVorbis(MixRampInfo &info, std::string_view entry) noexcept;
