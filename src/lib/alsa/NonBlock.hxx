// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "event/Chrono.hxx"
#include "util/ReusableArray.hxx"

#include <alsa/asoundlib.h>

#include <span>

class MultiSocketMonitor;

namespace Alsa {

class NonBlock {
	ReusableArray<pollfd> buffer;

public:
	std::span<pollfd> Allocate(std::size_t n) noexcept {
		return {buffer.Get(n), n};
	}

	std::span<pollfd> CopyReturnedEvents(MultiSocketMonitor &m,
					     std::size_t n) noexcept;
};

/**
 * Helper class for #MultiSocketMonitor's virtual methods which
 * manages the file descriptors for a #snd_pcm_t.
 */
class NonBlockPcm {
	NonBlock base;

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
class NonBlockMixer {
	NonBlock base;

public:
	Event::Duration PrepareSockets(MultiSocketMonitor &m,
				       snd_mixer_t *mixer) noexcept;

	/**
	 * Wrapper for snd_mixer_poll_descriptors_revents(), to be
	 * called from MultiSocketMonitor::DispatchSockets().
	 */
	void DispatchSockets(MultiSocketMonitor &m, snd_mixer_t *mixer) noexcept;
};

} // namespace Alsa
