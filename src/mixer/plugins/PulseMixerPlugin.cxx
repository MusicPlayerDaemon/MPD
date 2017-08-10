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
#include "PulseMixerPlugin.hxx"
#include "lib/pulse/Domain.hxx"
#include "lib/pulse/LogError.hxx"
#include "lib/pulse/LockGuard.hxx"
#include "mixer/MixerInternal.hxx"
#include "mixer/Listener.hxx"
#include "output/plugins/PulseOutputPlugin.hxx"

#include <pulse/context.h>
#include <pulse/introspect.h>
#include <pulse/stream.h>
#include <pulse/subscribe.h>

#include <stdexcept>

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
	int GetVolumeInternal();

	/* virtual methods from class Mixer */
	void Open() override {
	}

	void Close() noexcept override {
	}

	int GetVolume() override;
	void SetVolume(unsigned volume) override;
};

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

	listener.OnMixerVolumeChanged(*this, GetVolumeInternal());
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
		LogPulseError(context,
			      "pa_context_get_sink_input_info() failed");
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
		LogPulseError(context,
			      "pa_context_subscribe() failed");
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
		 gcc_unused const ConfigBlock &block)
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
PulseMixer::GetVolume()
{
	Pulse::LockGuard lock(pulse_output_get_mainloop(output));

	return GetVolumeInternal();
}

/**
 * Pulse mainloop lock must be held by caller
 */
int
PulseMixer::GetVolumeInternal()
{
	return online ?
		(int)((100 * (pa_cvolume_avg(&volume) + 1)) / PA_VOLUME_NORM)
		: -1;
}

void
PulseMixer::SetVolume(unsigned new_volume)
{
	Pulse::LockGuard lock(pulse_output_get_mainloop(output));

	if (!online)
		throw std::runtime_error("disconnected");

	struct pa_cvolume cvolume;
	pa_cvolume_set(&cvolume, volume.channels,
		       (new_volume * PA_VOLUME_NORM + 50) / 100);
	pulse_output_set_volume(output, &cvolume);
	volume = cvolume;
}

const MixerPlugin pulse_mixer_plugin = {
	pulse_mixer_init,
	false,
};
