// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_TAG_RVA2_HXX
#define MPD_TAG_RVA2_HXX

struct id3_tag;
struct ReplayGainInfo;

/**
 * Parse the RVA2 tag, and fill the #ReplayGainInfo struct.  This is
 * used by decoder plugins with ID3 support.
 *
 * @return true on success
 */
bool
tag_rva2_parse(const struct id3_tag *tag,
	       ReplayGainInfo &replay_gain_info) noexcept;

#endif
