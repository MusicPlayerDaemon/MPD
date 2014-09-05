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
#include "PulseOutputPlugin.hxx"
#include "../OutputAPI.hxx"
#include "mixer/MixerList.hxx"
#include "mixer/plugins/PulseMixerPlugin.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <pulse/thread-mainloop.h>
#include <pulse/context.h>
#include <pulse/stream.h>
#include <pulse/introspect.h>
#include <pulse/subscribe.h>
#include <pulse/error.h>
#include <pulse/version.h>

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#define MPD_PULSE_NAME "Music Player Daemon"

struct PulseOutput {
	AudioOutput base;

	const char *name;
	const char *server;
	const char *sink;

	PulseMixer *mixer;

	struct pa_threaded_mainloop *mainloop;
	struct pa_context *context;
	struct pa_stream *stream;

	size_t writable;

	PulseOutput()
		:base(pulse_output_plugin) {}
};

static constexpr Domain pulse_output_domain("pulse_output");

static void
SetError(Error &error, pa_context *context, const char *msg)
{
	const int e = pa_context_errno(context);
	error.Format(pulse_output_domain, e, "%s: %s", msg, pa_strerror(e));
}

void
pulse_output_lock(PulseOutput &po)
{
	pa_threaded_mainloop_lock(po.mainloop);
}

void
pulse_output_unlock(PulseOutput &po)
{
	pa_threaded_mainloop_unlock(po.mainloop);
}

void
pulse_output_set_mixer(PulseOutput &po, PulseMixer &pm)
{
	assert(po.mixer == nullptr);

	po.mixer = &pm;

	if (po.mainloop == nullptr)
		return;

	pa_threaded_mainloop_lock(po.mainloop);

	if (po.context != nullptr &&
	    pa_context_get_state(po.context) == PA_CONTEXT_READY) {
		pulse_mixer_on_connect(pm, po.context);

		if (po.stream != nullptr &&
		    pa_stream_get_state(po.stream) == PA_STREAM_READY)
			pulse_mixer_on_change(pm, po.context, po.stream);
	}

	pa_threaded_mainloop_unlock(po.mainloop);
}

void
pulse_output_clear_mixer(PulseOutput &po, gcc_unused PulseMixer &pm)
{
	assert(po.mixer == &pm);

	po.mixer = nullptr;
}

bool
pulse_output_set_volume(PulseOutput &po, const pa_cvolume *volume,
			Error &error)
{
	pa_operation *o;

	if (po.context == nullptr || po.stream == nullptr ||
	    pa_stream_get_state(po.stream) != PA_STREAM_READY) {
		error.Set(pulse_output_domain, "disconnected");
		return false;
	}

	o = pa_context_set_sink_input_volume(po.context,
					     pa_stream_get_index(po.stream),
					     volume, nullptr, nullptr);
	if (o == nullptr) {
		SetError(error, po.context,
			 "failed to set PulseAudio volume");
		return false;
	}

	pa_operation_unref(o);
	return true;
}

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
	assert(mainloop != nullptr);
	assert(operation != nullptr);

	pa_operation_state_t state;
	while ((state = pa_operation_get_state(operation))
	       == PA_OPERATION_RUNNING)
		pa_threaded_mainloop_wait(mainloop);

	pa_operation_unref(operation);

	return state == PA_OPERATION_DONE;
}

/**
 * Callback function for stream operation.  It just sends a signal to
 * the caller thread, to wake pulse_wait_for_operation() up.
 */
static void
pulse_output_stream_success_cb(gcc_unused pa_stream *s,
			       gcc_unused int success, void *userdata)
{
	PulseOutput *po = (PulseOutput *)userdata;

	pa_threaded_mainloop_signal(po->mainloop, 0);
}

static void
pulse_output_context_state_cb(struct pa_context *context, void *userdata)
{
	PulseOutput *po = (PulseOutput *)userdata;

	switch (pa_context_get_state(context)) {
	case PA_CONTEXT_READY:
		if (po->mixer != nullptr)
			pulse_mixer_on_connect(*po->mixer, context);

		pa_threaded_mainloop_signal(po->mainloop, 0);
		break;

	case PA_CONTEXT_TERMINATED:
	case PA_CONTEXT_FAILED:
		if (po->mixer != nullptr)
			pulse_mixer_on_disconnect(*po->mixer);

		/* the caller thread might be waiting for these
		   states */
		pa_threaded_mainloop_signal(po->mainloop, 0);
		break;

	case PA_CONTEXT_UNCONNECTED:
	case PA_CONTEXT_CONNECTING:
	case PA_CONTEXT_AUTHORIZING:
	case PA_CONTEXT_SETTING_NAME:
		break;
	}
}

static void
pulse_output_subscribe_cb(pa_context *context,
			  pa_subscription_event_type_t t,
			  uint32_t idx, void *userdata)
{
	PulseOutput *po = (PulseOutput *)userdata;
	pa_subscription_event_type_t facility =
		pa_subscription_event_type_t(t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK);
	pa_subscription_event_type_t type =
		pa_subscription_event_type_t(t & PA_SUBSCRIPTION_EVENT_TYPE_MASK);

	if (po->mixer != nullptr &&
	    facility == PA_SUBSCRIPTION_EVENT_SINK_INPUT &&
	    po->stream != nullptr &&
	    pa_stream_get_state(po->stream) == PA_STREAM_READY &&
	    idx == pa_stream_get_index(po->stream) &&
	    (type == PA_SUBSCRIPTION_EVENT_NEW ||
	     type == PA_SUBSCRIPTION_EVENT_CHANGE))
		pulse_mixer_on_change(*po->mixer, context, po->stream);
}

/**
 * Attempt to connect asynchronously to the PulseAudio server.
 *
 * @return true on success, false on error
 */
static bool
pulse_output_connect(PulseOutput *po, Error &error)
{
	assert(po != nullptr);
	assert(po->context != nullptr);

	if (pa_context_connect(po->context, po->server,
			       (pa_context_flags_t)0, nullptr) < 0) {
		SetError(error, po->context,
			 "pa_context_connect() has failed");
		return false;
	}

	return true;
}

/**
 * Frees and clears the stream.
 */
static void
pulse_output_delete_stream(PulseOutput *po)
{
	assert(po != nullptr);
	assert(po->stream != nullptr);

	pa_stream_set_suspended_callback(po->stream, nullptr, nullptr);

	pa_stream_set_state_callback(po->stream, nullptr, nullptr);
	pa_stream_set_write_callback(po->stream, nullptr, nullptr);

	pa_stream_disconnect(po->stream);
	pa_stream_unref(po->stream);
	po->stream = nullptr;
}

/**
 * Frees and clears the context.
 *
 * Caller must lock the main loop.
 */
static void
pulse_output_delete_context(PulseOutput *po)
{
	assert(po != nullptr);
	assert(po->context != nullptr);

	pa_context_set_state_callback(po->context, nullptr, nullptr);
	pa_context_set_subscribe_callback(po->context, nullptr, nullptr);

	pa_context_disconnect(po->context);
	pa_context_unref(po->context);
	po->context = nullptr;
}

/**
 * Create, set up and connect a context.
 *
 * Caller must lock the main loop.
 *
 * @return true on success, false on error
 */
static bool
pulse_output_setup_context(PulseOutput *po, Error &error)
{
	assert(po != nullptr);
	assert(po->mainloop != nullptr);

	po->context = pa_context_new(pa_threaded_mainloop_get_api(po->mainloop),
				     MPD_PULSE_NAME);
	if (po->context == nullptr) {
		error.Set(pulse_output_domain, "pa_context_new() has failed");
		return false;
	}

	pa_context_set_state_callback(po->context,
				      pulse_output_context_state_cb, po);
	pa_context_set_subscribe_callback(po->context,
					  pulse_output_subscribe_cb, po);

	if (!pulse_output_connect(po, error)) {
		pulse_output_delete_context(po);
		return false;
	}

	return true;
}

static AudioOutput *
pulse_output_init(const config_param &param, Error &error)
{
	PulseOutput *po;

	setenv("PULSE_PROP_media.role", "music", true);
	setenv("PULSE_PROP_application.icon_name", "mpd", true);

	po = new PulseOutput();
	if (!po->base.Configure(param, error)) {
		delete po;
		return nullptr;
	}

	po->name = param.GetBlockValue("name", "mpd_pulse");
	po->server = param.GetBlockValue("server");
	po->sink = param.GetBlockValue("sink");

	po->mixer = nullptr;
	po->mainloop = nullptr;
	po->context = nullptr;
	po->stream = nullptr;

	return &po->base;
}

static void
pulse_output_finish(AudioOutput *ao)
{
	PulseOutput *po = (PulseOutput *)ao;

	delete po;
}

static bool
pulse_output_enable(AudioOutput *ao, Error &error)
{
	PulseOutput *po = (PulseOutput *)ao;

	assert(po->mainloop == nullptr);
	assert(po->context == nullptr);

	/* create the libpulse mainloop and start the thread */

	po->mainloop = pa_threaded_mainloop_new();
	if (po->mainloop == nullptr) {
		error.Set(pulse_output_domain,
			  "pa_threaded_mainloop_new() has failed");
		return false;
	}

	pa_threaded_mainloop_lock(po->mainloop);

	if (pa_threaded_mainloop_start(po->mainloop) < 0) {
		pa_threaded_mainloop_unlock(po->mainloop);
		pa_threaded_mainloop_free(po->mainloop);
		po->mainloop = nullptr;

		error.Set(pulse_output_domain,
			  "pa_threaded_mainloop_start() has failed");
		return false;
	}

	/* create the libpulse context and connect it */

	if (!pulse_output_setup_context(po, error)) {
		pa_threaded_mainloop_unlock(po->mainloop);
		pa_threaded_mainloop_stop(po->mainloop);
		pa_threaded_mainloop_free(po->mainloop);
		po->mainloop = nullptr;
		return false;
	}

	pa_threaded_mainloop_unlock(po->mainloop);

	return true;
}

static void
pulse_output_disable(AudioOutput *ao)
{
	PulseOutput *po = (PulseOutput *)ao;

	assert(po->mainloop != nullptr);

	pa_threaded_mainloop_stop(po->mainloop);
	if (po->context != nullptr)
		pulse_output_delete_context(po);
	pa_threaded_mainloop_free(po->mainloop);
	po->mainloop = nullptr;
}

/**
 * Check if the context is (already) connected, and waits if not.  If
 * the context has been disconnected, retry to connect.
 *
 * Caller must lock the main loop.
 *
 * @return true on success, false on error
 */
static bool
pulse_output_wait_connection(PulseOutput *po, Error &error)
{
	assert(po->mainloop != nullptr);

	pa_context_state_t state;

	if (po->context == nullptr && !pulse_output_setup_context(po, error))
		return false;

	while (true) {
		state = pa_context_get_state(po->context);
		switch (state) {
		case PA_CONTEXT_READY:
			/* nothing to do */
			return true;

		case PA_CONTEXT_UNCONNECTED:
		case PA_CONTEXT_TERMINATED:
		case PA_CONTEXT_FAILED:
			/* failure */
			SetError(error, po->context, "failed to connect");
			pulse_output_delete_context(po);
			return false;

		case PA_CONTEXT_CONNECTING:
		case PA_CONTEXT_AUTHORIZING:
		case PA_CONTEXT_SETTING_NAME:
			/* wait some more */
			pa_threaded_mainloop_wait(po->mainloop);
			break;
		}
	}
}

static void
pulse_output_stream_suspended_cb(gcc_unused pa_stream *stream, void *userdata)
{
	PulseOutput *po = (PulseOutput *)userdata;

	assert(stream == po->stream || po->stream == nullptr);
	assert(po->mainloop != nullptr);

	/* wake up the main loop to break out of the loop in
	   pulse_output_play() */
	pa_threaded_mainloop_signal(po->mainloop, 0);
}

static void
pulse_output_stream_state_cb(pa_stream *stream, void *userdata)
{
	PulseOutput *po = (PulseOutput *)userdata;

	assert(stream == po->stream || po->stream == nullptr);
	assert(po->mainloop != nullptr);
	assert(po->context != nullptr);

	switch (pa_stream_get_state(stream)) {
	case PA_STREAM_READY:
		if (po->mixer != nullptr)
			pulse_mixer_on_change(*po->mixer, po->context, stream);

		pa_threaded_mainloop_signal(po->mainloop, 0);
		break;

	case PA_STREAM_FAILED:
	case PA_STREAM_TERMINATED:
		if (po->mixer != nullptr)
			pulse_mixer_on_disconnect(*po->mixer);

		pa_threaded_mainloop_signal(po->mainloop, 0);
		break;

	case PA_STREAM_UNCONNECTED:
	case PA_STREAM_CREATING:
		break;
	}
}

static void
pulse_output_stream_write_cb(gcc_unused pa_stream *stream, size_t nbytes,
			     void *userdata)
{
	PulseOutput *po = (PulseOutput *)userdata;

	assert(po->mainloop != nullptr);

	po->writable = nbytes;
	pa_threaded_mainloop_signal(po->mainloop, 0);
}

/**
 * Create, set up and connect a context.
 *
 * Caller must lock the main loop.
 *
 * @return true on success, false on error
 */
static bool
pulse_output_setup_stream(PulseOutput *po, const pa_sample_spec *ss,
			  Error &error)
{
	assert(po != nullptr);
	assert(po->context != nullptr);

	po->stream = pa_stream_new(po->context, po->name, ss, nullptr);
	if (po->stream == nullptr) {
		SetError(error, po->context, "pa_stream_new() has failed");
		return false;
	}

	pa_stream_set_suspended_callback(po->stream,
					 pulse_output_stream_suspended_cb, po);

	pa_stream_set_state_callback(po->stream,
				     pulse_output_stream_state_cb, po);
	pa_stream_set_write_callback(po->stream,
				     pulse_output_stream_write_cb, po);

	return true;
}

static bool
pulse_output_open(AudioOutput *ao, AudioFormat &audio_format,
		  Error &error)
{
	PulseOutput *po = (PulseOutput *)ao;
	pa_sample_spec ss;

	assert(po->mainloop != nullptr);

	pa_threaded_mainloop_lock(po->mainloop);

	if (po->context != nullptr) {
		switch (pa_context_get_state(po->context)) {
		case PA_CONTEXT_UNCONNECTED:
		case PA_CONTEXT_TERMINATED:
		case PA_CONTEXT_FAILED:
			/* the connection was closed meanwhile; delete
			   it, and pulse_output_wait_connection() will
			   reopen it */
			pulse_output_delete_context(po);
			break;

		case PA_CONTEXT_READY:
		case PA_CONTEXT_CONNECTING:
		case PA_CONTEXT_AUTHORIZING:
		case PA_CONTEXT_SETTING_NAME:
			break;
		}
	}

	if (!pulse_output_wait_connection(po, error)) {
		pa_threaded_mainloop_unlock(po->mainloop);
		return false;
	}

	/* MPD doesn't support the other pulseaudio sample formats, so
	   we just force MPD to send us everything as 16 bit */
	audio_format.format = SampleFormat::S16;

	ss.format = PA_SAMPLE_S16NE;
	ss.rate = audio_format.sample_rate;
	ss.channels = audio_format.channels;

	/* create a stream .. */

	if (!pulse_output_setup_stream(po, &ss, error)) {
		pa_threaded_mainloop_unlock(po->mainloop);
		return false;
	}

	/* .. and connect it (asynchronously) */

	if (pa_stream_connect_playback(po->stream, po->sink,
				       nullptr, pa_stream_flags_t(0),
				       nullptr, nullptr) < 0) {
		pulse_output_delete_stream(po);

		SetError(error, po->context,
			 "pa_stream_connect_playback() has failed");
		pa_threaded_mainloop_unlock(po->mainloop);
		return false;
	}

	pa_threaded_mainloop_unlock(po->mainloop);

	return true;
}

static void
pulse_output_close(AudioOutput *ao)
{
	PulseOutput *po = (PulseOutput *)ao;
	pa_operation *o;

	assert(po->mainloop != nullptr);

	pa_threaded_mainloop_lock(po->mainloop);

	if (pa_stream_get_state(po->stream) == PA_STREAM_READY) {
		o = pa_stream_drain(po->stream,
				    pulse_output_stream_success_cb, po);
		if (o == nullptr) {
			FormatWarning(pulse_output_domain,
				      "pa_stream_drain() has failed: %s",
				      pa_strerror(pa_context_errno(po->context)));
		} else
			pulse_wait_for_operation(po->mainloop, o);
	}

	pulse_output_delete_stream(po);

	if (po->context != nullptr &&
	    pa_context_get_state(po->context) != PA_CONTEXT_READY)
		pulse_output_delete_context(po);

	pa_threaded_mainloop_unlock(po->mainloop);
}

/**
 * Check if the stream is (already) connected, and waits if not.  The
 * mainloop must be locked before calling this function.
 *
 * @return true on success, false on error
 */
static bool
pulse_output_wait_stream(PulseOutput *po, Error &error)
{
	while (true) {
		switch (pa_stream_get_state(po->stream)) {
		case PA_STREAM_READY:
			return true;

		case PA_STREAM_FAILED:
		case PA_STREAM_TERMINATED:
		case PA_STREAM_UNCONNECTED:
			SetError(error, po->context,
				 "failed to connect the stream");
			return false;

		case PA_STREAM_CREATING:
			pa_threaded_mainloop_wait(po->mainloop);
			break;
		}
	}
}

/**
 * Sets cork mode on the stream.
 */
static bool
pulse_output_stream_pause(PulseOutput *po, bool pause,
			  Error &error)
{
	pa_operation *o;

	assert(po->mainloop != nullptr);
	assert(po->context != nullptr);
	assert(po->stream != nullptr);

	o = pa_stream_cork(po->stream, pause,
			   pulse_output_stream_success_cb, po);
	if (o == nullptr) {
		SetError(error, po->context, "pa_stream_cork() has failed");
		return false;
	}

	if (!pulse_wait_for_operation(po->mainloop, o)) {
		SetError(error, po->context, "pa_stream_cork() has failed");
		return false;
	}

	return true;
}

static unsigned
pulse_output_delay(AudioOutput *ao)
{
	PulseOutput *po = (PulseOutput *)ao;
	unsigned result = 0;

	pa_threaded_mainloop_lock(po->mainloop);

	if (po->base.pause && pa_stream_is_corked(po->stream) &&
	    pa_stream_get_state(po->stream) == PA_STREAM_READY)
		/* idle while paused */
		result = 1000;

	pa_threaded_mainloop_unlock(po->mainloop);

	return result;
}

static size_t
pulse_output_play(AudioOutput *ao, const void *chunk, size_t size,
		  Error &error)
{
	PulseOutput *po = (PulseOutput *)ao;

	assert(po->mainloop != nullptr);
	assert(po->stream != nullptr);

	pa_threaded_mainloop_lock(po->mainloop);

	/* check if the stream is (already) connected */

	if (!pulse_output_wait_stream(po, error)) {
		pa_threaded_mainloop_unlock(po->mainloop);
		return 0;
	}

	assert(po->context != nullptr);

	/* unpause if previously paused */

	if (pa_stream_is_corked(po->stream) &&
	    !pulse_output_stream_pause(po, false, error)) {
		pa_threaded_mainloop_unlock(po->mainloop);
		return 0;
	}

	/* wait until the server allows us to write */

	while (po->writable == 0) {
		if (pa_stream_is_suspended(po->stream)) {
			pa_threaded_mainloop_unlock(po->mainloop);
			error.Set(pulse_output_domain, "suspended");
			return 0;
		}

		pa_threaded_mainloop_wait(po->mainloop);

		if (pa_stream_get_state(po->stream) != PA_STREAM_READY) {
			pa_threaded_mainloop_unlock(po->mainloop);
			error.Set(pulse_output_domain, "disconnected");
			return 0;
		}
	}

	/* now write */

	if (size > po->writable)
		/* don't send more than possible */
		size = po->writable;

	po->writable -= size;

	int result = pa_stream_write(po->stream, chunk, size, nullptr,
				     0, PA_SEEK_RELATIVE);
	pa_threaded_mainloop_unlock(po->mainloop);
	if (result < 0) {
		SetError(error, po->context, "pa_stream_write() failed");
		return 0;
	}

	return size;
}

static void
pulse_output_cancel(AudioOutput *ao)
{
	PulseOutput *po = (PulseOutput *)ao;
	pa_operation *o;

	assert(po->mainloop != nullptr);
	assert(po->stream != nullptr);

	pa_threaded_mainloop_lock(po->mainloop);

	if (pa_stream_get_state(po->stream) != PA_STREAM_READY) {
		/* no need to flush when the stream isn't connected
		   yet */
		pa_threaded_mainloop_unlock(po->mainloop);
		return;
	}

	assert(po->context != nullptr);

	o = pa_stream_flush(po->stream, pulse_output_stream_success_cb, po);
	if (o == nullptr) {
		FormatWarning(pulse_output_domain,
			      "pa_stream_flush() has failed: %s",
			      pa_strerror(pa_context_errno(po->context)));
		pa_threaded_mainloop_unlock(po->mainloop);
		return;
	}

	pulse_wait_for_operation(po->mainloop, o);
	pa_threaded_mainloop_unlock(po->mainloop);
}

static bool
pulse_output_pause(AudioOutput *ao)
{
	PulseOutput *po = (PulseOutput *)ao;

	assert(po->mainloop != nullptr);
	assert(po->stream != nullptr);

	pa_threaded_mainloop_lock(po->mainloop);

	/* check if the stream is (already/still) connected */

	Error error;
	if (!pulse_output_wait_stream(po, error)) {
		pa_threaded_mainloop_unlock(po->mainloop);
		LogError(error);
		return false;
	}

	assert(po->context != nullptr);

	/* cork the stream */

	if (!pa_stream_is_corked(po->stream) &&
	    !pulse_output_stream_pause(po, true, error)) {
		pa_threaded_mainloop_unlock(po->mainloop);
		LogError(error);
		return false;
	}

	pa_threaded_mainloop_unlock(po->mainloop);

	return true;
}

static bool
pulse_output_test_default_device(void)
{
	bool success;

	const config_param empty;
	PulseOutput *po = (PulseOutput *)
		pulse_output_init(empty, IgnoreError());
	if (po == nullptr)
		return false;

	success = pulse_output_wait_connection(po, IgnoreError());
	pulse_output_finish(&po->base);

	return success;
}

const struct AudioOutputPlugin pulse_output_plugin = {
	"pulse",
	pulse_output_test_default_device,
	pulse_output_init,
	pulse_output_finish,
	pulse_output_enable,
	pulse_output_disable,
	pulse_output_open,
	pulse_output_close,
	pulse_output_delay,
	nullptr,
	pulse_output_play,
	nullptr,
	pulse_output_cancel,
	pulse_output_pause,

	&pulse_mixer_plugin,
};
