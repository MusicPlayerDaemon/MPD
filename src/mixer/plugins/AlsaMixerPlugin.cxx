/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "lib/alsa/NonBlock.hxx"
#include "mixer/MixerInternal.hxx"
#include "mixer/Listener.hxx"
#include "output/OutputAPI.hxx"
#include "event/MultiSocketMonitor.hxx"
#include "event/DeferredMonitor.hxx"
#include "event/Call.hxx"
#include "util/ASCII.hxx"
#include "util/ReusableArray.hxx"
#include "util/Domain.hxx"
#include "util/RuntimeError.hxx"
#include "Log.hxx"

extern "C" {
#include "volume_mapping.h"
}

#include <alsa/asoundlib.h>

#include <math.h>

#define VOLUME_MIXER_ALSA_DEFAULT		"default"
#define VOLUME_MIXER_ALSA_CONTROL_DEFAULT	"PCM"
static constexpr unsigned VOLUME_MIXER_ALSA_INDEX_DEFAULT = 0;

class AlsaMixerMonitor final : MultiSocketMonitor, DeferredMonitor {
	snd_mixer_t *mixer;

	ReusableArray<pollfd> pfd_buffer;

public:
	AlsaMixerMonitor(EventLoop &_loop, snd_mixer_t *_mixer)
		:MultiSocketMonitor(_loop), DeferredMonitor(_loop),
		 mixer(_mixer) {
		DeferredMonitor::Schedule();
	}

	~AlsaMixerMonitor() {
		BlockingCall(MultiSocketMonitor::GetEventLoop(), [this](){
				MultiSocketMonitor::Reset();
			});
	}

private:
	virtual void RunDeferred() override {
		InvalidateSockets();
	}

	virtual std::chrono::steady_clock::duration PrepareSockets() override;
	virtual void DispatchSockets() override;
};

class AlsaMixer final : public Mixer {
	EventLoop &event_loop;

	const char *device;
	const char *control;
	unsigned int index;

	snd_mixer_t *handle;
	snd_mixer_elem_t *elem;

	AlsaMixerMonitor *monitor;

public:
	AlsaMixer(EventLoop &_event_loop, MixerListener &_listener)
		:Mixer(alsa_mixer_plugin, _listener),
		 event_loop(_event_loop) {}

	virtual ~AlsaMixer();

	void Configure(const ConfigBlock &block);
	void Setup();

	/* virtual methods from class Mixer */
	void Open() override;
	void Close() override;
	int GetVolume() override;
	void SetVolume(unsigned volume) override;
};

static constexpr Domain alsa_mixer_domain("alsa_mixer");

std::chrono::steady_clock::duration
AlsaMixerMonitor::PrepareSockets()
{
	if (mixer == nullptr) {
		ClearSocketList();
		return std::chrono::steady_clock::duration(-1);
	}

	return PrepareAlsaMixerSockets(*this, mixer, pfd_buffer);
}

void
AlsaMixerMonitor::DispatchSockets()
{
	assert(mixer != nullptr);

	int err = snd_mixer_handle_events(mixer);
	if (err < 0) {
		FormatError(alsa_mixer_domain,
			    "snd_mixer_handle_events() failed: %s",
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

static int
alsa_mixer_elem_callback(snd_mixer_elem_t *elem, unsigned mask)
{
	AlsaMixer &mixer = *(AlsaMixer *)
		snd_mixer_elem_get_callback_private(elem);

	if (mask & SND_CTL_EVENT_MASK_VALUE) {
		try {
			int volume = mixer.GetVolume();
			mixer.listener.OnMixerVolumeChanged(mixer, volume);
		} catch (const std::runtime_error &) {
		}
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
alsa_mixer_init(EventLoop &event_loop, gcc_unused AudioOutput &ao,
		MixerListener &listener,
		const ConfigBlock &block)
{
	AlsaMixer *am = new AlsaMixer(event_loop, listener);
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
alsa_mixer_lookup_elem(snd_mixer_t *handle, const char *name, unsigned idx)
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
		throw FormatRuntimeError("failed to attach to %s: %s",
					 device, snd_strerror(err));

	if ((err = snd_mixer_selem_register(handle, nullptr, nullptr)) < 0)
		throw FormatRuntimeError("snd_mixer_selem_register() failed: %s",
					 snd_strerror(err));

	if ((err = snd_mixer_load(handle)) < 0)
		throw FormatRuntimeError("snd_mixer_load() failed: %s\n",
					 snd_strerror(err));

	elem = alsa_mixer_lookup_elem(handle, control, index);
	if (elem == nullptr)
		throw FormatRuntimeError("no such mixer control: %s", control);

	snd_mixer_elem_set_callback_private(elem, this);
	snd_mixer_elem_set_callback(elem, alsa_mixer_elem_callback);

	monitor = new AlsaMixerMonitor(event_loop, handle);
}

void
AlsaMixer::Open()
{
	int err;

	err = snd_mixer_open(&handle, 0);
	if (err < 0)
		throw FormatRuntimeError("snd_mixer_open() failed: %s",
					 snd_strerror(err));

	try {
		Setup();
	} catch (...) {
		snd_mixer_close(handle);
		throw;
	}
}

void
AlsaMixer::Close()
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
		throw FormatRuntimeError("snd_mixer_handle_events() failed: %s",
					 snd_strerror(err));

	return lrint(100 * get_normalized_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT));
}

void
AlsaMixer::SetVolume(unsigned volume)
{
	assert(handle != nullptr);

	int err = set_normalized_playback_volume(elem, 0.01*volume, 1);
	if (err < 0)
		throw FormatRuntimeError("failed to set ALSA volume: %s",
					 snd_strerror(err));
}

const MixerPlugin alsa_mixer_plugin = {
	alsa_mixer_init,
	true,
};
