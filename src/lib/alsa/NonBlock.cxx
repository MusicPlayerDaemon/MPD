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

#include "NonBlock.hxx"
#include "Error.hxx"
#include "event/MultiSocketMonitor.hxx"
#include "util/RuntimeError.hxx"

Event::Duration
AlsaNonBlockPcm::PrepareSockets(MultiSocketMonitor &m, snd_pcm_t *pcm)
{
	int count = snd_pcm_poll_descriptors_count(pcm);
	if (count <= 0) {
		if (count == 0)
			throw std::runtime_error("snd_pcm_poll_descriptors_count() failed");
		else
			throw Alsa::MakeError(count, "snd_pcm_poll_descriptors_count() failed");
	}

	struct pollfd *pfds = pfd_buffer.Get(count);

	count = snd_pcm_poll_descriptors(pcm, pfds, count);
	if (count <= 0) {
		if (count == 0)
			throw std::runtime_error("snd_pcm_poll_descriptors() failed");
		else
			throw Alsa::MakeError(count, "snd_pcm_poll_descriptors() failed");
	}

	m.ReplaceSocketList(pfds, count);
	return Event::Duration(-1);
}

void
AlsaNonBlockPcm::DispatchSockets(MultiSocketMonitor &m,
				 snd_pcm_t *pcm)
{
	int count = snd_pcm_poll_descriptors_count(pcm);
	if (count <= 0)
		return;

	const auto pfds = pfd_buffer.Get(count), end = pfds + count;

	auto *i = pfds;
	m.ForEachReturnedEvent([&i, end](SocketDescriptor s, unsigned events){
			if (i >= end)
				return;

			i->fd = s.Get();
			i->events = i->revents = events;
			++i;
		});

	unsigned short dummy;
	int err = snd_pcm_poll_descriptors_revents(pcm, pfds, i - pfds, &dummy);
	if (err < 0)
		throw Alsa::MakeError(err, "snd_pcm_poll_descriptors_revents() failed");
}

Event::Duration
AlsaNonBlockMixer::PrepareSockets(MultiSocketMonitor &m, snd_mixer_t *mixer) noexcept
{
	int count = snd_mixer_poll_descriptors_count(mixer);
	if (count <= 0) {
		m.ClearSocketList();
		return Event::Duration(-1);
	}

	struct pollfd *pfds = pfd_buffer.Get(count);

	count = snd_mixer_poll_descriptors(mixer, pfds, count);
	if (count < 0)
		count = 0;

	m.ReplaceSocketList(pfds, count);
	return Event::Duration(-1);
}

void
AlsaNonBlockMixer::DispatchSockets(MultiSocketMonitor &m,
				   snd_mixer_t *mixer) noexcept
{
	int count = snd_mixer_poll_descriptors_count(mixer);
	if (count <= 0)
		return;

	const auto pfds = pfd_buffer.Get(count), end = pfds + count;

	auto *i = pfds;
	m.ForEachReturnedEvent([&i, end](SocketDescriptor s, unsigned events){
			if (i >= end)
				return;

			i->fd = s.Get();
			i->events = i->revents = events;
			++i;
		});

	unsigned short dummy;
	snd_mixer_poll_descriptors_revents(mixer, pfds, i - pfds, &dummy);
}
