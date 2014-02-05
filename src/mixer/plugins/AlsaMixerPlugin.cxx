/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "mixer/MixerInternal.hxx"
#include "mixer/Listener.hxx"
#include "output/OutputAPI.hxx"
#include "event/MultiSocketMonitor.hxx"
#include "event/DeferredMonitor.hxx"
#include "event/Loop.hxx"
#include "util/ASCII.hxx"
#include "util/ReusableArray.hxx"
#include "util/Clamp.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <algorithm>

#include <alsa/asoundlib.h>

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

private:
	virtual void RunDeferred() override {
		InvalidateSockets();
	}

	virtual int PrepareSockets() override;
	virtual void DispatchSockets() override;
};

class AlsaMixer final : public Mixer {
	EventLoop &event_loop;

	const char *device;
	const char *control;
	unsigned int index;

	snd_mixer_t *handle;
	snd_mixer_elem_t *elem;
	long volume_min;
	long volume_max;
	int volume_set;

	AlsaMixerMonitor *monitor;

public:
	AlsaMixer(EventLoop &_event_loop, MixerListener &_listener)
		:Mixer(alsa_mixer_plugin, _listener),
		 event_loop(_event_loop) {}

	virtual ~AlsaMixer();

	void Configure(const config_param &param);
	bool Setup(Error &error);

	/* virtual methods from class Mixer */
	virtual bool Open(Error &error) override;
	virtual void Close() override;
	virtual int GetVolume(Error &error) override;
	virtual bool SetVolume(unsigned volume, Error &error) override;
};

static constexpr Domain alsa_mixer_domain("alsa_mixer");

int
AlsaMixerMonitor::PrepareSockets()
{
	if (mixer == nullptr) {
		ClearSocketList();
		return -1;
	}

	int count = snd_mixer_poll_descriptors_count(mixer);
	if (count < 0)
		count = 0;

	struct pollfd *pfds = pfd_buffer.Get(count);

	count = snd_mixer_poll_descriptors(mixer, pfds, count);
	if (count < 0)
		count = 0;

	ReplaceSocketList(pfds, count);
	return -1;
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
		int volume = mixer.GetVolume(IgnoreError());
		mixer.listener.OnMixerVolumeChanged(mixer, volume);
	}

	return 0;
}

/*
 * mixer_plugin methods
 *
 */

inline void
AlsaMixer::Configure(const config_param &param)
{
	device = param.GetBlockValue("mixer_device",
				     VOLUME_MIXER_ALSA_DEFAULT);
	control = param.GetBlockValue("mixer_control",
				      VOLUME_MIXER_ALSA_CONTROL_DEFAULT);
	index = param.GetBlockValue("mixer_index",
				    VOLUME_MIXER_ALSA_INDEX_DEFAULT);
}

static Mixer *
alsa_mixer_init(EventLoop &event_loop, gcc_unused AudioOutput &ao,
		MixerListener &listener,
		const config_param &param,
		gcc_unused Error &error)
{
	AlsaMixer *am = new AlsaMixer(event_loop, listener);
	am->Configure(param);

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

inline bool
AlsaMixer::Setup(Error &error)
{
	int err;

	if ((err = snd_mixer_attach(handle, device)) < 0) {
		error.Format(alsa_mixer_domain, err,
			     "failed to attach to %s: %s",
			     device, snd_strerror(err));
		return false;
	}

	if ((err = snd_mixer_selem_register(handle, nullptr,
		    nullptr)) < 0) {
		error.Format(alsa_mixer_domain, err,
			     "snd_mixer_selem_register() failed: %s",
			     snd_strerror(err));
		return false;
	}

	if ((err = snd_mixer_load(handle)) < 0) {
		error.Format(alsa_mixer_domain, err,
			     "snd_mixer_load() failed: %s\n",
			     snd_strerror(err));
		return false;
	}

	elem = alsa_mixer_lookup_elem(handle, control, index);
	if (elem == nullptr) {
		error.Format(alsa_mixer_domain, 0,
			    "no such mixer control: %s", control);
		return false;
	}

	snd_mixer_selem_get_playback_volume_range(elem, &volume_min,
						  &volume_max);

	snd_mixer_elem_set_callback_private(elem, this);
	snd_mixer_elem_set_callback(elem, alsa_mixer_elem_callback);

	monitor = new AlsaMixerMonitor(event_loop, handle);

	return true;
}

inline bool
AlsaMixer::Open(Error &error)
{
	int err;

	volume_set = -1;

	err = snd_mixer_open(&handle, 0);
	if (err < 0) {
		error.Format(alsa_mixer_domain, err,
			     "snd_mixer_open() failed: %s", snd_strerror(err));
		return false;
	}

	if (!Setup(error)) {
		snd_mixer_close(handle);
		return false;
	}

	return true;
}

inline void
AlsaMixer::Close()
{
	assert(handle != nullptr);

	delete monitor;

	snd_mixer_elem_set_callback(elem, nullptr);
	snd_mixer_close(handle);
}

inline int
AlsaMixer::GetVolume(Error &error)
{
	int err;
	int ret;
	long level;

	assert(handle != nullptr);

	err = snd_mixer_handle_events(handle);
	if (err < 0) {
		error.Format(alsa_mixer_domain, err,
			     "snd_mixer_handle_events() failed: %s",
			     snd_strerror(err));
		return false;
	}

	err = snd_mixer_selem_get_playback_volume(elem,
						  SND_MIXER_SCHN_FRONT_LEFT,
						  &level);
	if (err < 0) {
		error.Format(alsa_mixer_domain, err,
			     "failed to read ALSA volume: %s",
			     snd_strerror(err));
		return false;
	}

	ret = ((volume_set / 100.0) * (volume_max - volume_min)
	       + volume_min) + 0.5;
	if (volume_set > 0 && ret == level) {
		ret = volume_set;
	} else {
		ret = (int)(100 * (((float)(level - volume_min)) /
				   (volume_max - volume_min)) + 0.5);
	}

	return ret;
}

inline bool
AlsaMixer::SetVolume(unsigned volume, Error &error)
{
	float vol;
	long level;
	int err;

	assert(handle != nullptr);

	vol = volume;

	volume_set = vol + 0.5;

	level = (long)(((vol / 100.0) * (volume_max - volume_min) +
			volume_min) + 0.5);
	level = Clamp(level, volume_min, volume_max);

	err = snd_mixer_selem_set_playback_volume_all(elem, level);
	if (err < 0) {
		error.Format(alsa_mixer_domain, err,
			     "failed to set ALSA volume: %s",
			     snd_strerror(err));
		return false;
	}

	return true;
}

const MixerPlugin alsa_mixer_plugin = {
	alsa_mixer_init,
	true,
};
