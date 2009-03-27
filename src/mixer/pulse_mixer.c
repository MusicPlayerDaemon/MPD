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
#include "conf.h"

#include <glib.h>
#include <pulse/volume.h>
#include <pulse/pulseaudio.h>

#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "pulse_mixer"

struct pulse_mixer {
	struct mixer base;

	const char *server;
	const char *sink;
	const char *output_name;

	GMutex *mutex;

	uint32_t index;
	bool online;

	struct pa_context *context;
	struct pa_threaded_mainloop *mainloop;
	struct pa_cvolume volume;

};

/**
 * \brief waits for a pulseaudio operation to finish, frees it and
 *     unlocks the mainloop
 * \param operation the operation to wait for
 * \return true if operation has finished normally (DONE state),
 *     false otherwise
 */
static bool
pulse_wait_for_operation(struct pa_threaded_mainloop *mainloop,
			 struct pa_operation *operation)
{
	pa_operation_state_t state;

	assert(mainloop != NULL);
	assert(operation != NULL);

	pa_threaded_mainloop_lock(mainloop);

	state = pa_operation_get_state(operation);
	while (state == PA_OPERATION_RUNNING) {
		pa_threaded_mainloop_wait(mainloop);
		state = pa_operation_get_state(operation);
	}

	pa_operation_unref(operation);
	pa_threaded_mainloop_unlock(mainloop);

	return state == PA_OPERATION_DONE;
}

static void
sink_input_cb(G_GNUC_UNUSED pa_context *context, const pa_sink_input_info *i,
	      int eol, void *userdata)
{

	struct pulse_mixer *pm = userdata;

	if (eol) {
		g_debug("eol error sink_input_cb");
		return;
	}

	if (i == NULL) {
		g_debug("Sink input callback failure");
		return;
	}

	g_debug("sink input cb %s, index %d ",i->name,i->index);

	if (strcmp(i->name,pm->output_name) == 0) {
		g_mutex_lock(pm->mutex);

		pm->index = i->index;
		pm->online = true;
		pm->volume = i->volume;

		g_mutex_unlock(pm->mutex);
	} else
		g_debug("bad name");
}

static void
sink_input_vol(G_GNUC_UNUSED pa_context *context, const pa_sink_input_info *i,
	       int eol, void *userdata)
{

	struct pulse_mixer *pm = userdata;

	if (eol) {
		g_debug("eol error sink_input_vol");
		return;
	}

	if (i == NULL) {
		g_debug("Sink input callback failure");
		return;
	}

	g_debug("sink input vol %s, index %d ", i->name, i->index);

	g_mutex_lock(pm->mutex);
	pm->volume = i->volume;
	g_mutex_unlock(pm->mutex);

	pa_threaded_mainloop_signal(pm->mainloop, 0);
}

static void
subscribe_cb(pa_context *c, pa_subscription_event_type_t t,
	     uint32_t idx, void *userdata)
{

	struct pulse_mixer *pm = userdata;

	g_debug("subscribe call back");

	switch (t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {
	case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
		g_mutex_lock(pm->mutex);

		if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) ==
		    PA_SUBSCRIPTION_EVENT_REMOVE &&
		    pm->index == idx)
			pm->online = false;
		else {
			pa_operation *o;

			o = pa_context_get_sink_input_info(c, idx,
							   sink_input_cb, pm);
			if (o == NULL) {
				g_mutex_unlock(pm->mutex);
				g_debug("pa_context_get_sink_input_info() failed");
				return;
			}

			pa_operation_unref(o);
		}

		g_mutex_unlock(pm->mutex);
		break;
	}
}

static void
context_state_cb(pa_context *context, void *userdata)
{
	struct pulse_mixer *pm = userdata;

	switch (pa_context_get_state(context)) {
	case PA_CONTEXT_READY: {
		pa_operation *o;

		pa_context_set_subscribe_callback(context, subscribe_cb, pm);

		o = pa_context_subscribe(context,
					 (pa_subscription_mask_t)PA_SUBSCRIPTION_MASK_SINK_INPUT,
					 NULL, NULL);
		if (o == NULL) {
			g_debug("pa_context_subscribe() failed");
			return;
		}

		pa_operation_unref(o);

		o = pa_context_get_sink_input_info_list(context,
							sink_input_cb, pm);
		if (o == NULL) {
			g_debug("pa_context_get_sink_input_info_list() failed");
			return;
		}

		pa_operation_unref(o);

		pa_threaded_mainloop_signal(pm->mainloop, 0);
		break;
	}

	case PA_CONTEXT_UNCONNECTED:
	case PA_CONTEXT_CONNECTING:
	case PA_CONTEXT_AUTHORIZING:
	case PA_CONTEXT_SETTING_NAME:
		break;
	case PA_CONTEXT_TERMINATED:
	case PA_CONTEXT_FAILED:
		pa_threaded_mainloop_signal(pm->mainloop, 0);
		break;
	}
}


static struct mixer *
pulse_mixer_init(const struct config_param *param)
{
	struct pulse_mixer *pm = g_new(struct pulse_mixer,1);
	mixer_init(&pm->base, &pulse_mixer);

	pm->online = false;

	pm->server = config_get_block_string(param, "server", NULL);
	pm->sink = config_get_block_string(param, "sink", NULL);
	pm->output_name = config_get_block_string(param, "name", NULL);

	pm->mutex = g_mutex_new();

	return &pm->base;
}

static void
pulse_mixer_finish(struct mixer *data)
{
	struct pulse_mixer *pm = (struct pulse_mixer *) data;

	g_mutex_free(pm->mutex);
	g_free(pm);
}

static bool
pulse_mixer_setup(struct pulse_mixer *pm)
{
	pa_context_set_state_callback(pm->context, context_state_cb, pm);

	if (pa_context_connect(pm->context, pm->server,
			       (pa_context_flags_t)0, NULL) < 0) {
		g_debug("context server fail");
		return false;
	}

	pa_threaded_mainloop_lock(pm->mainloop);

	if (pa_threaded_mainloop_start(pm->mainloop) < 0) {
		pa_threaded_mainloop_unlock(pm->mainloop);
		g_debug("error start mainloop");
		return false;
	}

	pa_threaded_mainloop_wait(pm->mainloop);

	if (pa_context_get_state(pm->context) != PA_CONTEXT_READY) {
		pa_threaded_mainloop_unlock(pm->mainloop);
		g_debug("error context not ready");
		return false;
	}

	pa_threaded_mainloop_unlock(pm->mainloop);

	return true;
}

static bool
pulse_mixer_open(struct mixer *data)
{
	struct pulse_mixer *pm = (struct pulse_mixer *) data;

	g_debug("pulse mixer open");

	pm->index = 0;
	pm->online = false;

	pm->mainloop = pa_threaded_mainloop_new();
	if (pm->mainloop == NULL) {
		g_debug("failed mainloop");
		return false;
	}

	pm->context = pa_context_new(pa_threaded_mainloop_get_api(pm->mainloop),
				     "Mixer mpd");
	if (pm->context == NULL) {
		pa_threaded_mainloop_stop(pm->mainloop);
		pa_threaded_mainloop_free(pm->mainloop);
		g_debug("failed context");
		return false;
	}

	if (!pulse_mixer_setup(pm)) {
		pa_threaded_mainloop_stop(pm->mainloop);
		pa_context_disconnect(pm->context);
		pa_context_unref(pm->context);
		pa_threaded_mainloop_free(pm->mainloop);
		return false;
	}

	return true;
}

static void
pulse_mixer_close(struct mixer *data)
{
	struct pulse_mixer *pm = (struct pulse_mixer *) data;

	pa_threaded_mainloop_stop(pm->mainloop);
	pa_context_disconnect(pm->context);
	pa_context_unref(pm->context);
	pa_threaded_mainloop_free(pm->mainloop);

	pm->online = false;
}

static int
pulse_mixer_get_volume(struct mixer *mixer)
{
	struct pulse_mixer *pm = (struct pulse_mixer *) mixer;
	int ret;
	pa_operation *o;

	g_mutex_lock(pm->mutex);
	if (!pm->online) {
		g_mutex_unlock(pm->mutex);
		return false;
	}

	o = pa_context_get_sink_input_info(pm->context, pm->index,
					   sink_input_vol, pm);
	g_mutex_unlock(pm->mutex);
	if (o == NULL) {
		g_debug("pa_context_get_sink_input_info() failed");
		return false;
	}

	if (!pulse_wait_for_operation(pm->mainloop, o))
		return false;

	g_mutex_lock(pm->mutex);
	ret = pm->online
		? (int)((100*(pa_cvolume_avg(&pm->volume)+1))/PA_VOLUME_NORM)
		: -1;
	g_mutex_unlock(pm->mutex);

	return ret;
}

static bool
pulse_mixer_set_volume(struct mixer *mixer, unsigned volume)
{
	struct pulse_mixer *pm = (struct pulse_mixer *) mixer;
	struct pa_cvolume cvolume;
	pa_operation *o;

	g_mutex_lock(pm->mutex);
	if (!pm->online) {
		g_mutex_unlock(pm->mutex);
		return false;
	}

	pa_cvolume_set(&cvolume, pm->volume.channels,
		       (pa_volume_t)volume * PA_VOLUME_NORM / 100 + 0.5);

	o = pa_context_set_sink_input_volume(pm->context, pm->index,
					     &cvolume, NULL, NULL);
	g_mutex_unlock(pm->mutex);
	if (o == NULL) {
		g_debug("pa_context_set_sink_input_volume() failed");
		return false;
	}

	pa_operation_unref(o);

	return true;
}

const struct mixer_plugin pulse_mixer = {
	.init = pulse_mixer_init,
	.finish = pulse_mixer_finish,
	.open = pulse_mixer_open,
	.close = pulse_mixer_close,
	.get_volume = pulse_mixer_get_volume,
	.set_volume = pulse_mixer_set_volume,
};
