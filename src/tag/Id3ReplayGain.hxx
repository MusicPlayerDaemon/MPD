// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_TAG_ID3_REPLAY_GAIN_HXX
#define MPD_TAG_ID3_REPLAY_GAIN_HXX

struct id3_tag;
struct ReplayGainInfo;

bool
Id3ToReplayGainInfo(ReplayGainInfo &rgi, const struct id3_tag *tag) noexcept;

#endif
