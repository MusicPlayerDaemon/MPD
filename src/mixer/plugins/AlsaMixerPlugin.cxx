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

#include "lib/alsa/NonBlock.hxx"
#include "lib/alsa/Error.hxx"
#include "mixer/MixerInternal.hxx"
#include "mixer/Listener.hxx"
#include "output/OutputAPI.hxx"
#include "event/MultiSocketMonitor.hxx"
#include "event/InjectEvent.hxx"
#include "event/Call.hxx"
#include "util/ASCII.hxx"
#include "util/Domain.hxx"
#include "util/Math.hxx"
#include "util/RuntimeError.hxx"
#include "Log.hxx"

extern "C" {
#include "volume_mapping.h"
}

#include <alsa/asoundlib.h>

#define VOLUME_MIXER_ALSA_DEFAULT		"default"
#define VOLUME_MIXER_ALSA_CONTROL_DEFAULT	"PCM"
static constexpr unsigned VOLUME_MIXER_ALSA_INDEX_DEFAULT = 0;

class AlsaMixerMonitor final : MultiSocketMonitor {
	InjectEvent defer_invalidate_sockets;

	snd_mixer_t *mixer;

	AlsaNonBlockMixer non_block;

public:
	AlsaMixerMonitor(EventLoop &_loop, snd_mixer_t *_mixer)
		:MultiSocketMonitor(_loop),
		 defer_invalidate_sockets(_loop,
					  BIND_THIS_METHOD(InvalidateSockets)),
		 mixer(_mixer) {
		defer_invalidate_sockets.Schedule();
	}

	~AlsaMixerMonitor() {
		BlockingCall(MultiSocketMonitor::GetEventLoop(), [this](){
				MultiSocketMonitor::Reset();
				defer_invalidate_sockets.Cancel();
			});
	}

	AlsaMixerMonitor(const AlsaMixerMonitor &) = delete;
	AlsaMixerMonitor &operator=(const AlsaMixerMonitor &) = delete;

private:
	Event::Duration PrepareSockets() noexcept override;
	void DispatchSockets() noexcept override;
};

class AlsaMixer final : public Mixer {
	EventLoop &event_loop;

	const char *device;
	const char *control;
	unsigned int index;

	snd_mixer_t *handle;
	snd_mixer_elem_t *elem;

	AlsaMixerMonitor *monitor;

	/**
	 * These fields are our workaround for rounding errors when
	 * the resolution of a mixer knob isn't fine enough to
	 * represent all 101 possible values (0..100).
	 *
	 * "desired_volume" is the percent value passed to
	 * SetVolume(), and "resulting_volume" is the volume which was
	 * actually set, and would be returned by the next
	 * GetPercentVolume() call.
	 *
	 * When GetVolume() is called, we compare the
	 * "resulting_volume" with the value returned by
	 * GetPercentVolume(), and if it's the same, we're still on
	 * the same value that was previously set (but may have been
	 * rounded down or up).
	 */
	int desired_volume, resulting_volume;

public:
	AlsaMixer(EventLoop &_event_loop, MixerListener &_listener)
		:Mixer(alsa_mixer_plugin, _listener),
		 event_loop(_event_loop) {}

	~AlsaMixer() override;

	AlsaMixer(const AlsaMixer &) = delete;
	AlsaMixer &operator=(const AlsaMixer &) = delete;

	void Configure(const ConfigBlock &block);
	void Setup();

	/* virtual methods from class Mixer */
	void Open() override;
	void Close() noexcept override;
	int GetVolume() override;
	void SetVolume(unsigned volume) override;

private:
	[[gnu::const]]
	static unsigned NormalizedToPercent(double normalized) noexcept {
		return lround(100 * normalized);
	}

	[[gnu::pure]]
	[[nodiscard]] double GetNormalizedVolume() const noexcept {
		return get_normalized_playback_volume(elem,
						      SND_MIXER_SCHN_FRONT_LEFT);
	}

	[[gnu::pure]]
	[[nodiscard]] unsigned GetPercentVolume() const noexcept {
		return NormalizedToPercent(GetNormalizedVolume());
	}

	static int ElemCallback(snd_mixer_elem_t *elem,
				unsigned mask) noexcept;

};

static constexpr Domain alsa_mixer_domain("alsa_mixer");

Event::Duration
AlsaMixerMonitor::PrepareSockets() noexcept
{
	if (mixer == nullptr) {
		ClearSocketList();
		return Event::Duration(-1);
	}

	return non_block.PrepareSockets(*this, mixer);
}

void
AlsaMixerMonitor::DispatchSockets() noexcept
{
	assert(mixer != nullptr);

	non_block.DispatchSockets(*this, mixer);

	int err = snd_mixer_handle_events(mixer);
	if (err < 0) {
		FmtError(alsa_mixer_domain,
			 "snd_mixer_handle_events() failed: {}",
			 snd_strerror(err));

		if (err == -ENODEV) {
			/* the sound device was unplugged; disable
			   this GSource */
			mixer = nullptr;
			InvalidateSockets();
			return;
		}
	}
}

/*
 * libasound callbacks
 *
 */

int
AlsaMixer::ElemCallback(snd_mixer_elem_t *elem, unsigned mask) noexcept
{
	AlsaMixer &mixer = *(AlsaMixer *)
		snd_mixer_elem_get_callback_private(elem);

	if (mask & SND_CTL_EVENT_MASK_VALUE) {
		int volume = mixer.GetPercentVolume();

		if (mixer.resulting_volume >= 0 &&
		    volume == mixer.resulting_volume)
			/* still the same volume (this might be a
			   callback caused by SetVolume()) - switch to
			   desired_volume */
			volume = mixer.desired_volume;
		else
			/* flush */
			mixer.desired_volume = mixer.resulting_volume = -1;

		mixer.listener.OnMixerVolumeChanged(mixer, volume);
	}

	return 0;
}

/*
 * mixer_plugin methods
 *
 */

inline void
AlsaMixer::Configure(const ConfigBlock &block)
{
	device = block.GetBlockValue("mixer_device",
				     VOLUME_MIXER_ALSA_DEFAULT);
	control = block.GetBlockValue("mixer_control",
				      VOLUME_MIXER_ALSA_CONTROL_DEFAULT);
	index = block.GetBlockValue("mixer_index",
				    VOLUME_MIXER_ALSA_INDEX_DEFAULT);
}

static Mixer *
alsa_mixer_init(EventLoop &event_loop, [[maybe_unused]] AudioOutput &ao,
		MixerListener &listener,
		const ConfigBlock &block)
{
	auto *am = new AlsaMixer(event_loop, listener);
	am->Configure(block);

	return am;
}

AlsaMixer::~AlsaMixer()
{
	/* free libasound's config cache */
	snd_config_update_free_global();
}

gcc_pure
static snd_mixer_elem_t *
alsa_mixer_lookup_elem(snd_mixer_t *handle,
		       const char *name, unsigned idx) noexcept
{
	for (snd_mixer_elem_t *elem = snd_mixer_first_elem(handle);
	     elem != nullptr; elem = snd_mixer_elem_next(elem)) {
		if (snd_mixer_elem_get_type(elem) == SND_MIXER_ELEM_SIMPLE &&
		    StringEqualsCaseASCII(snd_mixer_selem_get_name(elem),
					  name) &&
		    snd_mixer_selem_get_index(elem) == idx)
			return elem;
	}

	return nullptr;
}

inline void
AlsaMixer::Setup()
{
	int err;

	if ((err = snd_mixer_attach(handle, device)) < 0)
		throw Alsa::MakeError(err,
				      fmt::format("failed to attach to {}",
						  device).c_str());

	if ((err = snd_mixer_selem_register(handle, nullptr, nullptr)) < 0)
		throw Alsa::MakeError(err, "snd_mixer_selem_register() failed");

	if ((err = snd_mixer_load(handle)) < 0)
		throw Alsa::MakeError(err, "snd_mixer_load() failed");

	elem = alsa_mixer_lookup_elem(handle, control, index);
	if (elem == nullptr)
		throw FormatRuntimeError("no such mixer control: %s", control);

	snd_mixer_elem_set_callback_private(elem, this);
	snd_mixer_elem_set_callback(elem, ElemCallback);

	monitor = new AlsaMixerMonitor(event_loop, handle);
}

void
AlsaMixer::Open()
{
	desired_volume = resulting_volume = -1;

	int err;

	err = snd_mixer_open(&handle, 0);
	if (err < 0)
		throw Alsa::MakeError(err, "snd_mixer_open() failed");

	try {
		Setup();
	} catch (...) {
		snd_mixer_close(handle);
		throw;
	}
}

void
AlsaMixer::Close() noexcept
{
	assert(handle != nullptr);

	delete monitor;

	snd_mixer_elem_set_callback(elem, nullptr);
	snd_mixer_close(handle);
}

int
AlsaMixer::GetVolume()
{
	int err;

	assert(handle != nullptr);

	err = snd_mixer_handle_events(handle);
	if (err < 0)
		throw Alsa::MakeError(err, "snd_mixer_handle_events() failed");

	int volume = GetPercentVolume();
	if (resulting_volume >= 0 && volume == resulting_volume)
		/* we're still on the value passed to SetVolume() */
		volume = desired_volume;

	return volume;
}

void
AlsaMixer::SetVolume(unsigned volume)
{
	assert(handle != nullptr);

	int err = set_normalized_playback_volume(elem, 0.01*volume, 1);
	if (err < 0)
		throw Alsa::MakeError(err, "failed to set ALSA volume");

	desired_volume = volume;
	resulting_volume = GetPercentVolume();
}

const MixerPlugin alsa_mixer_plugin = {
	alsa_mixer_init,
	true,
};
