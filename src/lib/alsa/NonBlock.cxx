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

#include "config.h"
#include "NonBlock.hxx"
#include "event/MultiSocketMonitor.hxx"
#include "util/RuntimeError.hxx"

std::chrono::steady_clock::duration
PrepareAlsaPcmSockets(MultiSocketMonitor &m, snd_pcm_t *pcm,
		      ReusableArray<pollfd> &pfd_buffer)
{
	int count = snd_pcm_poll_descriptors_count(pcm);
	if (count <= 0) {
		if (count == 0)
			throw std::runtime_error("snd_pcm_poll_descriptors_count() failed");
		else
			throw FormatRuntimeError("snd_pcm_poll_descriptors_count() failed: %s",
						 snd_strerror(-count));
	}

	struct pollfd *pfds = pfd_buffer.Get(count);

	count = snd_pcm_poll_descriptors(pcm, pfds, count);
	if (count <= 0) {
		if (count == 0)
			throw std::runtime_error("snd_pcm_poll_descriptors() failed");
		else
			throw FormatRuntimeError("snd_pcm_poll_descriptors() failed: %s",
						 snd_strerror(-count));
	}

	m.ReplaceSocketList(pfds, count);
	return std::chrono::steady_clock::duration(-1);
}

std::chrono::steady_clock::duration
PrepareAlsaMixerSockets(MultiSocketMonitor &m, snd_mixer_t *mixer,
			ReusableArray<pollfd> &pfd_buffer) noexcept
{
	int count = snd_mixer_poll_descriptors_count(mixer);
	if (count <= 0) {
		m.ClearSocketList();
		return std::chrono::steady_clock::duration(-1);
	}

	struct pollfd *pfds = pfd_buffer.Get(count);

	count = snd_mixer_poll_descriptors(mixer, pfds, count);
	if (count < 0)
		count = 0;

	m.ReplaceSocketList(pfds, count);
	return std::chrono::steady_clock::duration(-1);
}
