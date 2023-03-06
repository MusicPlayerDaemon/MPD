// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_MIXER_LISTENER_HXX
#define MPD_MIXER_LISTENER_HXX

class Mixer;

/**
 * An interface that listens on events from mixer plugins.  The
 * methods must be thread-safe and non-blocking.
 */
class MixerListener {
public:
	virtual void OnMixerVolumeChanged(Mixer &mixer,
					  int volume) noexcept = 0;
	virtual void OnMixerChanged() noexcept = 0;
};

#endif
