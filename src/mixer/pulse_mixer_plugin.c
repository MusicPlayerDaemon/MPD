/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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

#include "mixer_api.h"
#include "output/pulse_output_plugin.h"
#include "conf.h"
#include "event_pipe.h"

#include <glib.h>

#include <pulse/thread-mainloop.h>
#include <pulse/context.h>
#include <pulse/introspect.h>
#include <pulse/stream.h>
#include <pulse/subscribe.h>
#include <pulse/error.h>

#include <assert.h>
#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "pulse_mixer"

struct pulse_mixer {
	struct mixer base;

	struct pulse_output *output;

	bool online;
	struct pa_cvolume volume;

};

/**
 * The quark used for GError.domain.
 */
static inline GQuark
pulse_mixer_quark(void)
{
	return g_quark_from_static_string("pulse_mixer");
}

static void
pulse_mixer_offline(struct pulse_mixer *pm)
{
	if (!pm->online)
		return;

	pm->online = false;

	event_pipe_emit(PIPE_EVENT_MIXER);
}

/**
 * Callback invoked by pulse_mixer_update().  Receives the new mixer
 * value.
 */
static void
pulse_mixer_volume_cb(G_GNUC_UNUSED pa_context *context, const pa_sink_input_info *i,
		      int eol, void *userdata)
{
	struct pulse_mixer *pm = userdata;

	if (eol)
		return;

	if (i == NULL) {
		pulse_mixer_offline(pm);
		return;
	}

	pm->online = true;
	pm->volume = i->volume;

	event_pipe_emit(PIPE_EVENT_MIXER);
}

static void
pulse_mixer_update(struct pulse_mixer *pm)
{
	pa_operation *o;

	assert(pm->output->stream != NULL);

	if (pm->output->context == NULL ||
	    pa_stream_get_state(pm->output->stream) != PA_STREAM_READY)
		return;

	o = pa_context_get_sink_input_info(pm->output->context,
					   pa_stream_get_index(pm->output->stream),
					   pulse_mixer_volume_cb, pm);
	if (o == NULL) {
		g_warning("pa_context_get_sink_input_info() failed: %s",
			  pa_strerror(pa_context_errno(pm->output->context)));
		pulse_mixer_offline(pm);
		return;
	}

	pa_operation_unref(o);
}

static void
pulse_mixer_handle_sink_input(struct pulse_mixer *pm,
			      pa_subscription_event_type_t t,
			      uint32_t idx)
{
	if (pm->output->stream == NULL) {
		pulse_mixer_offline(pm);
		return;
	}

	if (idx != pa_stream_get_index(pm->output->stream))
		return;

	if (t == PA_SUBSCRIPTION_EVENT_NEW ||
	    t == PA_SUBSCRIPTION_EVENT_CHANGE)
		pulse_mixer_update(pm);
}

static void
pulse_mixer_subscribe_cb(G_GNUC_UNUSED pa_context *c, pa_subscription_event_type_t t,
	     uint32_t idx, void *userdata)
{
	struct pulse_mixer *pm = userdata;

	switch (t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {
	case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
		pulse_mixer_handle_sink_input(pm,
					      t & PA_SUBSCRIPTION_EVENT_TYPE_MASK,
					      idx);
		break;
	}
}

static void
pulxe_mixer_context_state_cb(pa_context *context, void *userdata)
{
	struct pulse_mixer *pm = userdata;
	pa_operation *o;

	/* pass event to the output's callback function */
	pulse_output_context_state_cb(context, pm->output);

	if (pa_context_get_state(context) == PA_CONTEXT_READY) {
		/* subscribe to sink_input events after the connection
		   has been established */

		o = pa_context_subscribe(context,
					 (pa_subscription_mask_t)PA_SUBSCRIPTION_MASK_SINK_INPUT,
					 NULL, NULL);
		if (o == NULL) {
			g_warning("pa_context_subscribe() failed: %s",
				  pa_strerror(pa_context_errno(context)));
			return;
		}

		pa_operation_unref(o);

		if (pm->output->stream != NULL)
			pulse_mixer_update(pm);
	}
}

static struct mixer *
pulse_mixer_init(void *ao, G_GNUC_UNUSED const struct config_param *param,
		 GError **error_r)
{
	struct pulse_mixer *pm;

	if (ao == NULL) {
		g_set_error(error_r, pulse_mixer_quark(), 0,
			    "The pulse mixer cannot work without the audio output");
		return false;
	}

	pm = g_new(struct pulse_mixer,1);
	mixer_init(&pm->base, &pulse_mixer_plugin);

	pm->output = ao;
	pm->online = false;

	pa_threaded_mainloop_lock(pm->output->mainloop);

	/* register callbacks (override the output's context state
	   callback) */

	pa_context_set_state_callback(pm->output->context,
				      pulxe_mixer_context_state_cb, pm);
	pa_context_set_subscribe_callback(pm->output->context,
					  pulse_mixer_subscribe_cb, pm);

	/* check the current state now (we might have missed the first
	   events!) */
	pulxe_mixer_context_state_cb(pm->output->context, pm);

	pa_threaded_mainloop_unlock(pm->output->mainloop);

	return &pm->base;
}

static void
pulse_mixer_finish(struct mixer *data)
{
	struct pulse_mixer *pm = (struct pulse_mixer *) data;

	/* restore callbacks */

	pa_threaded_mainloop_lock(pm->output->mainloop);

	if (pm->output->context != NULL) {
		pa_context_set_state_callback(pm->output->context,
					      pulse_output_context_state_cb,
					      pm->output);
		pa_context_set_subscribe_callback(pm->output->context,
						  NULL, NULL);
	}

	pa_threaded_mainloop_unlock(pm->output->mainloop);

	/* free resources */

	g_free(pm);
}

static bool
pulse_mixer_open(struct mixer *data, G_GNUC_UNUSED GError **error_r)
{
	struct pulse_mixer *pm = (struct pulse_mixer *) data;

	pa_threaded_mainloop_lock(pm->output->mainloop);
	if (pm->output->stream != NULL)
		pulse_mixer_update(pm);
	pa_threaded_mainloop_unlock(pm->output->mainloop);

	return true;
}

static void
pulse_mixer_close(struct mixer *data)
{
	struct pulse_mixer *pm = (struct pulse_mixer *) data;

	pulse_mixer_offline(pm);
}

static int
pulse_mixer_get_volume(struct mixer *mixer, G_GNUC_UNUSED GError **error_r)
{
	struct pulse_mixer *pm = (struct pulse_mixer *) mixer;
	int ret;

	pa_threaded_mainloop_lock(pm->output->mainloop);

	ret = pm->online
		? (int)((100*(pa_cvolume_avg(&pm->volume)+1))/PA_VOLUME_NORM)
		: -1;

	pa_threaded_mainloop_unlock(pm->output->mainloop);

	return ret;
}

static bool
pulse_mixer_set_volume(struct mixer *mixer, unsigned volume, GError **error_r)
{
	struct pulse_mixer *pm = (struct pulse_mixer *) mixer;
	struct pa_cvolume cvolume;
	pa_operation *o;

	pa_threaded_mainloop_lock(pm->output->mainloop);

	if (!pm->online || pm->output->stream == NULL ||
	    pm->output->context == NULL) {
		pa_threaded_mainloop_unlock(pm->output->mainloop);
		g_set_error(error_r, pulse_mixer_quark(), 0, "disconnected");
		return false;
	}

	pa_cvolume_set(&cvolume, pm->volume.channels,
		       (pa_volume_t)volume * PA_VOLUME_NORM / 100 + 0.5);

	o = pa_context_set_sink_input_volume(pm->output->context,
					     pa_stream_get_index(pm->output->stream),
					     &cvolume, NULL, NULL);
	pa_threaded_mainloop_unlock(pm->output->mainloop);
	if (o == NULL) {
		g_set_error(error_r, pulse_mixer_quark(), 0,
			    "failed to set PulseAudio volume");
		return false;
	}

	pa_operation_unref(o);

	return true;
}

const struct mixer_plugin pulse_mixer_plugin = {
	.init = pulse_mixer_init,
	.finish = pulse_mixer_finish,
	.open = pulse_mixer_open,
	.close = pulse_mixer_close,
	.get_volume = pulse_mixer_get_volume,
	.set_volume = pulse_mixer_set_volume,
};
