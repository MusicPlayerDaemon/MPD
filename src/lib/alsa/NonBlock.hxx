/*
 * Copyright 2003-2018 The Music Player Daemon Project
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

#include "check.h"
#include "util/ReusableArray.hxx"

#include <alsa/asoundlib.h>

#include <chrono>

class MultiSocketMonitor;

/**
 * Update #MultiSocketMonitor's socket list from
 * snd_pcm_poll_descriptors().  To be called from
 * MultiSocketMonitor::PrepareSockets().
 *
 * Throws exception on error.
 */
std::chrono::steady_clock::duration
PrepareAlsaPcmSockets(MultiSocketMonitor &m, snd_pcm_t *pcm,
		      ReusableArray<pollfd> &pfd_buffer);

/**
 * Update #MultiSocketMonitor's socket list from
 * snd_mixer_poll_descriptors().  To be called from
 * MultiSocketMonitor::PrepareSockets().
 */
std::chrono::steady_clock::duration
PrepareAlsaMixerSockets(MultiSocketMonitor &m, snd_mixer_t *mixer,
		      ReusableArray<pollfd> &pfd_buffer) noexcept;

#endif
