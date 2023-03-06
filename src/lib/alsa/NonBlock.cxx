// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "NonBlock.hxx"
#include "Error.hxx"
#include "event/MultiSocketMonitor.hxx"

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
