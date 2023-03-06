// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef NULL_MIXER_LISTENER_HXX
#define NULL_MIXER_LISTENER_HXX

#include "mixer/Listener.hxx"

class NullMixerListener : public MixerListener {
public:
	void OnMixerVolumeChanged(Mixer &, int) noexcept override {}
	virtual void OnMixerChanged() noexcept override {}
};

#endif
