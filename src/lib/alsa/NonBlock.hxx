// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_ALSA_NON_BLOCK_HXX
#define MPD_ALSA_NON_BLOCK_HXX

#include "event/Chrono.hxx"
#include "util/ReusableArray.hxx"

#include <alsa/asoundlib.h>

class MultiSocketMonitor;

/**
 * Helper class for #MultiSocketMonitor's virtual methods which
 * manages the file descriptors for a #snd_pcm_t.
 */
class AlsaNonBlockPcm {
	ReusableArray<pollfd> pfd_buffer;

public:
	/**
	 * Throws on error.
	 */
	Event::Duration PrepareSockets(MultiSocketMonitor &m,
				       snd_pcm_t *pcm);

	/**
	 * Wrapper for snd_pcm_poll_descriptors_revents(), to be
	 * called from MultiSocketMonitor::DispatchSockets().
	 *
	 * Throws on error.
	 */
	void DispatchSockets(MultiSocketMonitor &m, snd_pcm_t *pcm);
};

/**
 * Helper class for #MultiSocketMonitor's virtual methods which
 * manages the file descriptors for a #snd_mixer_t.
 */
class AlsaNonBlockMixer {
	ReusableArray<pollfd> pfd_buffer;

public:
	Event::Duration PrepareSockets(MultiSocketMonitor &m,
				       snd_mixer_t *mixer) noexcept;

	/**
	 * Wrapper for snd_mixer_poll_descriptors_revents(), to be
	 * called from MultiSocketMonitor::DispatchSockets().
	 */
	void DispatchSockets(MultiSocketMonitor &m, snd_mixer_t *mixer) noexcept;
};

#endif
