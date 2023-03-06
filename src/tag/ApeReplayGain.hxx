// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_APE_REPLAY_GAIN_HXX
#define MPD_APE_REPLAY_GAIN_HXX

class InputStream;
struct ReplayGainInfo;

/**
 * Throws on I/O error.
 */
bool
replay_gain_ape_read(InputStream &is, ReplayGainInfo &info);

#endif
