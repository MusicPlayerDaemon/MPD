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
#include "PulseMixerPlugin.hxx"
#include "mixer/MixerInternal.hxx"
#include "mixer/Listener.hxx"
#include "output/plugins/PulseOutputPlugin.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <pulse/context.h>
#include <pulse/introspect.h>
#include <pulse/stream.h>
#include <pulse/subscribe.h>
#include <pulse/error.h>

#include <assert.h>

class PulseMixer final : public Mixer {
	PulseOutput &output;

	bool online;
	struct pa_cvolume volume;

public:
	PulseMixer(PulseOutput &_output, MixerListener &_listener)
		:Mixer(pulse_mixer_plugin, _listener),
		 output(_output), online(false)
	{
	}

	virtual ~PulseMixer();

	void Offline();
	void VolumeCallback(const pa_sink_input_info *i, int eol);
	void Update(pa_context *context, pa_stream *stream);
	int GetVolumeInternal(Error &error);

	/* virtual methods from class Mixer */
	virtual bool Open(gcc_unused Error &error) override {
		return true;
	}

	virtual void Close() override {
	}

	virtual int GetVolume(Error &error) override;
	virtual bool SetVolume(unsigned volume, Error &error) override;
};

static constexpr Domain pulse_mixer_domain("pulse_mixer");

void
PulseMixer::Offline()
{
	if (!online)
		return;

	online = false;

	listener.OnMixerVolumeChanged(*this, -1);
}

inline void
PulseMixer::VolumeCallback(const pa_sink_input_info *i, int eol)
{
	if (eol)
		return;

	if (i == nullptr) {
		Offline();
		return;
	}

	online = true;
	volume = i->volume;

	listener.OnMixerVolumeChanged(*this, GetVolumeInternal(IgnoreError()));
}

/**
 * Callback invoked by pulse_mixer_update().  Receives the new mixer
 * value.
 */
static void
pulse_mixer_volume_cb(gcc_unused pa_context *context, const pa_sink_input_info *i,
		      int eol, void *userdata)
{
	PulseMixer *pm = (PulseMixer *)userdata;
	pm->VolumeCallback(i, eol);
}

inline void
PulseMixer::Update(pa_context *context, pa_stream *stream)
{
	assert(context != nullptr);
	assert(stream != nullptr);
	assert(pa_stream_get_state(stream) == PA_STREAM_READY);

	pa_operation *o =
		pa_context_get_sink_input_info(context,
					       pa_stream_get_index(stream),
					       pulse_mixer_volume_cb, this);
	if (o == nullptr) {
		FormatError(pulse_mixer_domain,
			    "pa_context_get_sink_input_info() failed: %s",
			    pa_strerror(pa_context_errno(context)));
		Offline();
		return;
	}

	pa_operation_unref(o);
}

void
pulse_mixer_on_connect(gcc_unused PulseMixer &pm,
		       struct pa_context *context)
{
	pa_operation *o;

	assert(context != nullptr);

	o = pa_context_subscribe(context,
				 (pa_subscription_mask_t)PA_SUBSCRIPTION_MASK_SINK_INPUT,
				 nullptr, nullptr);
	if (o == nullptr) {
		FormatError(pulse_mixer_domain,
			    "pa_context_subscribe() failed: %s",
			    pa_strerror(pa_context_errno(context)));
		return;
	}

	pa_operation_unref(o);
}

void
pulse_mixer_on_disconnect(PulseMixer &pm)
{
	pm.Offline();
}

void
pulse_mixer_on_change(PulseMixer &pm,
		      struct pa_context *context, struct pa_stream *stream)
{
	pm.Update(context, stream);
}

static Mixer *
pulse_mixer_init(gcc_unused EventLoop &event_loop, AudioOutput &ao,
		 MixerListener &listener,
		 gcc_unused const config_param &param,
		 gcc_unused Error &error)
{
	PulseOutput &po = (PulseOutput &)ao;
	PulseMixer *pm = new PulseMixer(po, listener);

	pulse_output_set_mixer(po, *pm);

	return pm;
}

PulseMixer::~PulseMixer()
{
	pulse_output_clear_mixer(output, *this);
}

int
PulseMixer::GetVolume(gcc_unused Error &error)
{
	pulse_output_lock(output);

	int result = GetVolumeInternal(error);
	pulse_output_unlock(output);

	return result;
}

/**
 * Pulse mainloop lock must be held by caller
 */
int
PulseMixer::GetVolumeInternal(gcc_unused Error &error)
{
	return online ?
		(int)((100 * (pa_cvolume_avg(&volume) + 1)) / PA_VOLUME_NORM)
		: -1;
}

bool
PulseMixer::SetVolume(unsigned new_volume, Error &error)
{
	pulse_output_lock(output);

	if (!online) {
		pulse_output_unlock(output);
		error.Set(pulse_mixer_domain, "disconnected");
		return false;
	}

	struct pa_cvolume cvolume;
	pa_cvolume_set(&cvolume, volume.channels,
		       (pa_volume_t)new_volume * PA_VOLUME_NORM / 100 + 0.5);
	bool success = pulse_output_set_volume(output, &cvolume, error);
	if (success)
		volume = cvolume;

	pulse_output_unlock(output);
	return success;
}

const MixerPlugin pulse_mixer_plugin = {
	pulse_mixer_init,
	false,
};
