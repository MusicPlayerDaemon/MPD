// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "NonBlock.hxx"
#include "Error.hxx"
#include "event/MultiSocketMonitor.hxx"

namespace Alsa {

std::span<pollfd>
NonBlock::CopyReturnedEvents(MultiSocketMonitor &m, std::size_t n) noexcept
{
	const auto pfds = buffer.Get(n), end = pfds + n;

	auto *i = pfds;
	m.ForEachReturnedEvent([&i, end](SocketDescriptor s, unsigned events){
		if (i >= end)
			return;

		i->fd = s.Get();
		i->events = i->revents = events;
		++i;
	});

	return {pfds, static_cast<std::size_t>(i - pfds)};

}

Event::Duration
NonBlockPcm::PrepareSockets(MultiSocketMonitor &m, snd_pcm_t *pcm)
{
	int count = snd_pcm_poll_descriptors_count(pcm);
	if (count <= 0) {
		if (count == 0)
			throw std::runtime_error("snd_pcm_poll_descriptors_count() failed");
		else
			throw Alsa::MakeError(count, "snd_pcm_poll_descriptors_count() failed");
	}

	const auto pfds = base.Allocate(count);

	count = snd_pcm_poll_descriptors(pcm, pfds.data(), count);
	if (count <= 0) {
		if (count == 0)
			throw std::runtime_error("snd_pcm_poll_descriptors() failed");
		else
			throw Alsa::MakeError(count, "snd_pcm_poll_descriptors() failed");
	}

	m.ReplaceSocketList(pfds.first(count));
	return Event::Duration(-1);
}

void
NonBlockPcm::DispatchSockets(MultiSocketMonitor &m, snd_pcm_t *pcm)
{
	int count = snd_pcm_poll_descriptors_count(pcm);
	if (count <= 0)
		return;

	const auto pfds = base.CopyReturnedEvents(m, count);

	unsigned short dummy;
	int err = snd_pcm_poll_descriptors_revents(pcm, pfds.data(), pfds.size(), &dummy);
	if (err < 0)
		throw Alsa::MakeError(err, "snd_pcm_poll_descriptors_revents() failed");
}

Event::Duration
NonBlockMixer::PrepareSockets(MultiSocketMonitor &m, snd_mixer_t *mixer) noexcept
{
	int count = snd_mixer_poll_descriptors_count(mixer);
	if (count <= 0) {
		m.ClearSocketList();
		return Event::Duration(-1);
	}

	const auto pfds = base.Allocate(count);

	count = snd_mixer_poll_descriptors(mixer, pfds.data(), count);
	if (count < 0)
		count = 0;

	m.ReplaceSocketList(pfds.first(count));
	return Event::Duration(-1);
}

void
NonBlockMixer::DispatchSockets(MultiSocketMonitor &m, snd_mixer_t *mixer) noexcept
{
	int count = snd_mixer_poll_descriptors_count(mixer);
	if (count <= 0)
		return;

	const auto pfds = base.CopyReturnedEvents(m, count);

	unsigned short dummy;
	snd_mixer_poll_descriptors_revents(mixer, pfds.data(), pfds.size(), &dummy);
}

} // namespace Alsa
