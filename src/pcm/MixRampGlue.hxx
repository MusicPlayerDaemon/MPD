// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <string>

struct AudioFormat;
class MusicPipe;

enum class MixRampDirection {
	START, END
};

[[gnu::pure]]
std::string
AnalyzeMixRamp(const MusicPipe &pipe, const AudioFormat &audio_format,
	       MixRampDirection direction) noexcept;
