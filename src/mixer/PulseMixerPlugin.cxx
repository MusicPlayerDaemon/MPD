/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "MixerInternal.hxx"
#include "output/PulseOutputPlugin.hxx"
#include "GlobalEvents.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <pulse/context.h>
#include <pulse/introspect.h>
#include <pulse/stream.h>
#include <pulse/subscribe.h>
#include <pulse/error.h>

#include <assert.h>

struct PulseMixer final : public Mixer {
	PulseOutput *output;

	bool online;
	struct pa_cvolume volume;

	PulseMixer(PulseOutput *_output)
		:Mixer(pulse_mixer_plugin),
		output(_output), online(false)
	{
	}
};

static constexpr Domain pulse_mixer_domain("pulse_mixer");

static void
pulse_mixer_offline(PulseMixer *pm)
{
	if (!pm->online)
		return;

	pm->online = false;

	GlobalEvents::Emit(GlobalEvents::MIXER);
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

	if (eol)
		return;

	if (i == nullptr) {
		pulse_mixer_offline(pm);
		return;
	}

	pm->online = true;
	pm->volume = i->volume;

	GlobalEvents::Emit(GlobalEvents::MIXER);
}

static void
pulse_mixer_update(PulseMixer *pm,
		   struct pa_context *context, struct pa_stream *stream)
{
	pa_operation *o;

	assert(context != nullptr);
	assert(stream != nullptr);
	assert(pa_stream_get_state(stream) == PA_STREAM_READY);

	o = pa_context_get_sink_input_info(context,
					   pa_stream_get_index(stream),
					   pulse_mixer_volume_cb, pm);
	if (o == nullptr) {
		FormatError(pulse_mixer_domain,
			    "pa_context_get_sink_input_info() failed: %s",
			    pa_strerror(pa_context_errno(context)));
		pulse_mixer_offline(pm);
		return;
	}

	pa_operation_unref(o);
}

void
pulse_mixer_on_connect(gcc_unused PulseMixer *pm,
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
pulse_mixer_on_disconnect(PulseMixer *pm)
{
	pulse_mixer_offline(pm);
}

void
pulse_mixer_on_change(PulseMixer *pm,
		      struct pa_context *context, struct pa_stream *stream)
{
	pulse_mixer_update(pm, context, stream);
}

static Mixer *
pulse_mixer_init(void *ao, gcc_unused const config_param &param,
		 Error &error)
{
	PulseOutput *po = (PulseOutput *)ao;

	if (ao == nullptr) {
		error.Set(pulse_mixer_domain,
			  "The pulse mixer cannot work without the audio output");
		return nullptr;
	}

	PulseMixer *pm = new PulseMixer(po);

	pulse_output_set_mixer(po, pm);

	return pm;
}

static void
pulse_mixer_finish(Mixer *data)
{
	PulseMixer *pm = (PulseMixer *) data;

	pulse_output_clear_mixer(pm->output, pm);

	delete pm;
}

static int
pulse_mixer_get_volume(Mixer *mixer, gcc_unused Error &error)
{
	PulseMixer *pm = (PulseMixer *) mixer;
	int ret;

	pulse_output_lock(pm->output);

	ret = pm->online
		? (int)((100*(pa_cvolume_avg(&pm->volume)+1))/PA_VOLUME_NORM)
		: -1;

	pulse_output_unlock(pm->output);

	return ret;
}

static bool
pulse_mixer_set_volume(Mixer *mixer, unsigned volume, Error &error)
{
	PulseMixer *pm = (PulseMixer *) mixer;
	struct pa_cvolume cvolume;
	bool success;

	pulse_output_lock(pm->output);

	if (!pm->online) {
		pulse_output_unlock(pm->output);
		error.Set(pulse_mixer_domain, "disconnected");
		return false;
	}

	pa_cvolume_set(&cvolume, pm->volume.channels,
		       (pa_volume_t)volume * PA_VOLUME_NORM / 100 + 0.5);
	success = pulse_output_set_volume(pm->output, &cvolume, error);
	if (success)
		pm->volume = cvolume;

	pulse_output_unlock(pm->output);

	return success;
}

const struct mixer_plugin pulse_mixer_plugin = {
	pulse_mixer_init,
	pulse_mixer_finish,
	nullptr,
	nullptr,
	pulse_mixer_get_volume,
	pulse_mixer_set_volume,
	false,
};
