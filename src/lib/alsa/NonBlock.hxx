/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

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
