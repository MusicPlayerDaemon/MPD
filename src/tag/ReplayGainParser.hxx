// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_TAG_REPLAY_GAIN_HXX
#define MPD_TAG_REPLAY_GAIN_HXX

#include <string_view>

struct ReplayGainInfo;

bool
ParseReplayGainTag(ReplayGainInfo &info,
		   const char *name, const char *value) noexcept;

bool
ParseReplayGainVorbis(ReplayGainInfo &info, std::string_view entry) noexcept;

#endif
