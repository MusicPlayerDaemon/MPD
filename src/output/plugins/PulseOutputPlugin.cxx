/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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
#include "lib/pulse/Domain.hxx"
#include "lib/pulse/Error.hxx"
#include "lib/pulse/LogError.hxx"
#include "../OutputAPI.hxx"
#include "../Wrapper.hxx"
#include "mixer/MixerList.hxx"
#include "mixer/plugins/PulseMixerPlugin.hxx"
#include "util/Error.hxx"
#include "Log.hxx"

#include <pulse/thread-mainloop.h>
#include <pulse/context.h>
#include <pulse/stream.h>
#include <pulse/introspect.h>
#include <pulse/subscribe.h>
#include <pulse/version.h>

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#define MPD_PULSE_NAME "Music Player Daemon"

class PulseOutput {
	friend struct AudioOutputWrapper<PulseOutput>;

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
		:base(pulse_output_plugin),
		 mixer(nullptr),
		 mainloop(nullptr), stream(nullptr) {}

public:
	void SetMixer(PulseMixer &_mixer);

	void ClearMixer(gcc_unused PulseMixer &old_mixer) {
		assert(mixer == &old_mixer);

		mixer = nullptr;
	}

	bool SetVolume(const pa_cvolume &volume, Error &error);

	void Lock() {
		pa_threaded_mainloop_lock(mainloop);
	}

	void Unlock() {
		pa_threaded_mainloop_unlock(mainloop);
	}

	void OnContextStateChanged(pa_context_state_t new_state);
	void OnServerLayoutChanged(pa_subscription_event_type_t t,
				   uint32_t idx);
	void OnStreamSuspended(pa_stream *_stream);
	void OnStreamStateChanged(pa_stream *_stream,
				  pa_stream_state_t new_state);
	void OnStreamWrite(size_t nbytes);

	void OnStreamSuccess() {
		Signal();
	}

	gcc_const
	static bool TestDefaultDevice();

	bool Configure(const config_param &param, Error &error);
	static PulseOutput *Create(const config_param &param, Error &error);

	bool Enable(Error &error);
	void Disable();

	bool Open(AudioFormat &audio_format, Error &error);
	void Close();

	unsigned Delay();
	size_t Play(const void *chunk, size_t size, Error &error);
	void Cancel();
	bool Pause();

private:
	/**
	 * Attempt to connect asynchronously to the PulseAudio server.
	 *
	 * @return true on success, false on error
	 */
	bool Connect(Error &error);

	/**
	 * Create, set up and connect a context.
	 *
	 * Caller must lock the main loop.
	 *
	 * @return true on success, false on error
	 */
	bool SetupContext(Error &error);

	/**
	 * Frees and clears the context.
	 *
	 * Caller must lock the main loop.
	 */
	void DeleteContext();

	void Signal() {
		pa_threaded_mainloop_signal(mainloop, 0);
	}

	/**
	 * Check if the context is (already) connected, and waits if
	 * not.  If the context has been disconnected, retry to
	 * connect.
	 *
	 * Caller must lock the main loop.
	 *
	 * @return true on success, false on error
	 */
	bool WaitConnection(Error &error);

	/**
	 * Create, set up and connect a context.
	 *
	 * Caller must lock the main loop.
	 *
	 * @return true on success, false on error
	 */
	bool SetupStream(const pa_sample_spec &ss, Error &error);

	/**
	 * Frees and clears the stream.
	 */
	void DeleteStream();

	/**
	 * Check if the stream is (already) connected, and waits if
	 * not.  The mainloop must be locked before calling this
	 * function.
	 *
	 * @return true on success, false on error
	 */
	bool WaitStream(Error &error);

	/**
	 * Sets cork mode on the stream.
	 */
	bool StreamPause(bool pause, Error &error);
};

void
pulse_output_lock(PulseOutput &po)
{
	po.Lock();
}

void
pulse_output_unlock(PulseOutput &po)
{
	po.Unlock();
}

inline void
PulseOutput::SetMixer(PulseMixer &_mixer)
{
	assert(mixer == nullptr);

	mixer = &_mixer;

	if (mainloop == nullptr)
		return;

	pa_threaded_mainloop_lock(mainloop);

	if (context != nullptr &&
	    pa_context_get_state(context) == PA_CONTEXT_READY) {
		pulse_mixer_on_connect(_mixer, context);

		if (stream != nullptr &&
		    pa_stream_get_state(stream) == PA_STREAM_READY)
			pulse_mixer_on_change(_mixer, context, stream);
	}

	pa_threaded_mainloop_unlock(mainloop);
}

void
pulse_output_set_mixer(PulseOutput &po, PulseMixer &pm)
{
	po.SetMixer(pm);
}

void
pulse_output_clear_mixer(PulseOutput &po, PulseMixer &pm)
{
	po.ClearMixer(pm);
}

inline bool
PulseOutput::SetVolume(const pa_cvolume &volume, Error &error)
{
	if (context == nullptr || stream == nullptr ||
	    pa_stream_get_state(stream) != PA_STREAM_READY) {
		error.Set(pulse_domain, "disconnected");
		return false;
	}

	pa_operation *o =
		pa_context_set_sink_input_volume(context,
						 pa_stream_get_index(stream),
						 &volume, nullptr, nullptr);
	if (o == nullptr) {
		SetPulseError(error, context,
			      "failed to set PulseAudio volume");
		return false;
	}

	pa_operation_unref(o);
	return true;
}

bool
pulse_output_set_volume(PulseOutput &po, const pa_cvolume *volume,
			Error &error)
{
	return po.SetVolume(*volume, error);
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
	PulseOutput &po = *(PulseOutput *)userdata;

	po.OnStreamSuccess();
}

inline void
PulseOutput::OnContextStateChanged(pa_context_state_t new_state)
{
	switch (new_state) {
	case PA_CONTEXT_READY:
		if (mixer != nullptr)
			pulse_mixer_on_connect(*mixer, context);

		Signal();
		break;

	case PA_CONTEXT_TERMINATED:
	case PA_CONTEXT_FAILED:
		if (mixer != nullptr)
			pulse_mixer_on_disconnect(*mixer);

		/* the caller thread might be waiting for these
		   states */
		Signal();
		break;

	case PA_CONTEXT_UNCONNECTED:
	case PA_CONTEXT_CONNECTING:
	case PA_CONTEXT_AUTHORIZING:
	case PA_CONTEXT_SETTING_NAME:
		break;
	}
}

static void
pulse_output_context_state_cb(struct pa_context *context, void *userdata)
{
	PulseOutput &po = *(PulseOutput *)userdata;

	po.OnContextStateChanged(pa_context_get_state(context));
}

inline void
PulseOutput::OnServerLayoutChanged(pa_subscription_event_type_t t,
				   uint32_t idx)
{
	pa_subscription_event_type_t facility =
		pa_subscription_event_type_t(t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK);
	pa_subscription_event_type_t type =
		pa_subscription_event_type_t(t & PA_SUBSCRIPTION_EVENT_TYPE_MASK);

	if (mixer != nullptr &&
	    facility == PA_SUBSCRIPTION_EVENT_SINK_INPUT &&
	    stream != nullptr &&
	    pa_stream_get_state(stream) == PA_STREAM_READY &&
	    idx == pa_stream_get_index(stream) &&
	    (type == PA_SUBSCRIPTION_EVENT_NEW ||
	     type == PA_SUBSCRIPTION_EVENT_CHANGE))
		pulse_mixer_on_change(*mixer, context, stream);
}

static void
pulse_output_subscribe_cb(gcc_unused pa_context *context,
			  pa_subscription_event_type_t t,
			  uint32_t idx, void *userdata)
{
	PulseOutput &po = *(PulseOutput *)userdata;

	po.OnServerLayoutChanged(t, idx);
}

inline bool
PulseOutput::Connect(Error &error)
{
	assert(context != nullptr);

	if (pa_context_connect(context, server,
			       (pa_context_flags_t)0, nullptr) < 0) {
		SetPulseError(error, context,
			      "pa_context_connect() has failed");
		return false;
	}

	return true;
}

void
PulseOutput::DeleteStream()
{
	assert(stream != nullptr);

	pa_stream_set_suspended_callback(stream, nullptr, nullptr);

	pa_stream_set_state_callback(stream, nullptr, nullptr);
	pa_stream_set_write_callback(stream, nullptr, nullptr);

	pa_stream_disconnect(stream);
	pa_stream_unref(stream);
	stream = nullptr;
}

void
PulseOutput::DeleteContext()
{
	assert(context != nullptr);

	pa_context_set_state_callback(context, nullptr, nullptr);
	pa_context_set_subscribe_callback(context, nullptr, nullptr);

	pa_context_disconnect(context);
	pa_context_unref(context);
	context = nullptr;
}

bool
PulseOutput::SetupContext(Error &error)
{
	assert(mainloop != nullptr);

	context = pa_context_new(pa_threaded_mainloop_get_api(mainloop),
				 MPD_PULSE_NAME);
	if (context == nullptr) {
		error.Set(pulse_domain, "pa_context_new() has failed");
		return false;
	}

	pa_context_set_state_callback(context,
				      pulse_output_context_state_cb, this);
	pa_context_set_subscribe_callback(context,
					  pulse_output_subscribe_cb, this);

	if (!Connect(error)) {
		DeleteContext();
		return false;
	}

	return true;
}

inline bool
PulseOutput::Configure(const config_param &param, Error &error)
{
	if (!base.Configure(param, error))
		return false;

	name = param.GetBlockValue("name", "mpd_pulse");
	server = param.GetBlockValue("server");
	sink = param.GetBlockValue("sink");

	return true;
}

PulseOutput *
PulseOutput::Create(const config_param &param, Error &error)
{
	setenv("PULSE_PROP_media.role", "music", true);
	setenv("PULSE_PROP_application.icon_name", "mpd", true);

	auto *po = new PulseOutput();
	if (!po->Configure(param, error)) {
		delete po;
		return nullptr;
	}

	return po;
}

inline bool
PulseOutput::Enable(Error &error)
{
	assert(mainloop == nullptr);

	/* create the libpulse mainloop and start the thread */

	mainloop = pa_threaded_mainloop_new();
	if (mainloop == nullptr) {
		error.Set(pulse_domain,
			  "pa_threaded_mainloop_new() has failed");
		return false;
	}

	pa_threaded_mainloop_lock(mainloop);

	if (pa_threaded_mainloop_start(mainloop) < 0) {
		pa_threaded_mainloop_unlock(mainloop);
		pa_threaded_mainloop_free(mainloop);
		mainloop = nullptr;

		error.Set(pulse_domain,
			  "pa_threaded_mainloop_start() has failed");
		return false;
	}

	/* create the libpulse context and connect it */

	if (!SetupContext(error)) {
		pa_threaded_mainloop_unlock(mainloop);
		pa_threaded_mainloop_stop(mainloop);
		pa_threaded_mainloop_free(mainloop);
		mainloop = nullptr;
		return false;
	}

	pa_threaded_mainloop_unlock(mainloop);

	return true;
}

inline void
PulseOutput::Disable()
{
	assert(mainloop != nullptr);

	pa_threaded_mainloop_stop(mainloop);
	if (context != nullptr)
		DeleteContext();
	pa_threaded_mainloop_free(mainloop);
	mainloop = nullptr;
}

bool
PulseOutput::WaitConnection(Error &error)
{
	assert(mainloop != nullptr);

	pa_context_state_t state;

	if (context == nullptr && !SetupContext(error))
		return false;

	while (true) {
		state = pa_context_get_state(context);
		switch (state) {
		case PA_CONTEXT_READY:
			/* nothing to do */
			return true;

		case PA_CONTEXT_UNCONNECTED:
		case PA_CONTEXT_TERMINATED:
		case PA_CONTEXT_FAILED:
			/* failure */
			SetPulseError(error, context, "failed to connect");
			DeleteContext();
			return false;

		case PA_CONTEXT_CONNECTING:
		case PA_CONTEXT_AUTHORIZING:
		case PA_CONTEXT_SETTING_NAME:
			/* wait some more */
			pa_threaded_mainloop_wait(mainloop);
			break;
		}
	}
}

inline void
PulseOutput::OnStreamSuspended(gcc_unused pa_stream *_stream)
{
	assert(_stream == stream || stream == nullptr);
	assert(mainloop != nullptr);

	/* wake up the main loop to break out of the loop in
	   pulse_output_play() */
	Signal();
}

static void
pulse_output_stream_suspended_cb(pa_stream *stream, void *userdata)
{
	PulseOutput &po = *(PulseOutput *)userdata;

	po.OnStreamSuspended(stream);
}

inline void
PulseOutput::OnStreamStateChanged(pa_stream *_stream,
				  pa_stream_state_t new_state)
{
	assert(_stream == stream || stream == nullptr);
	assert(mainloop != nullptr);
	assert(context != nullptr);

	switch (new_state) {
	case PA_STREAM_READY:
		if (mixer != nullptr)
			pulse_mixer_on_change(*mixer, context, _stream);

		Signal();
		break;

	case PA_STREAM_FAILED:
	case PA_STREAM_TERMINATED:
		if (mixer != nullptr)
			pulse_mixer_on_disconnect(*mixer);

		Signal();
		break;

	case PA_STREAM_UNCONNECTED:
	case PA_STREAM_CREATING:
		break;
	}
}

static void
pulse_output_stream_state_cb(pa_stream *stream, void *userdata)
{
	PulseOutput &po = *(PulseOutput *)userdata;

	return po.OnStreamStateChanged(stream, pa_stream_get_state(stream));
}

inline void
PulseOutput::OnStreamWrite(size_t nbytes)
{
	assert(mainloop != nullptr);

	writable = nbytes;
	Signal();
}

static void
pulse_output_stream_write_cb(gcc_unused pa_stream *stream, size_t nbytes,
			     void *userdata)
{
	PulseOutput &po = *(PulseOutput *)userdata;

	return po.OnStreamWrite(nbytes);
}

inline bool
PulseOutput::SetupStream(const pa_sample_spec &ss, Error &error)
{
	assert(context != nullptr);

	/* WAVE-EX is been adopted as the speaker map for most media files */
	pa_channel_map chan_map;
	pa_channel_map_init_auto(&chan_map, ss.channels,
				 PA_CHANNEL_MAP_WAVEEX);
	stream = pa_stream_new(context, name, &ss, &chan_map);
	if (stream == nullptr) {
		SetPulseError(error, context,
			      "pa_stream_new() has failed");
		return false;
	}

	pa_stream_set_suspended_callback(stream,
					 pulse_output_stream_suspended_cb,
					 this);

	pa_stream_set_state_callback(stream,
				     pulse_output_stream_state_cb, this);
	pa_stream_set_write_callback(stream,
				     pulse_output_stream_write_cb, this);

	return true;
}

inline bool
PulseOutput::Open(AudioFormat &audio_format, Error &error)
{
	assert(mainloop != nullptr);

	pa_threaded_mainloop_lock(mainloop);

	if (context != nullptr) {
		switch (pa_context_get_state(context)) {
		case PA_CONTEXT_UNCONNECTED:
		case PA_CONTEXT_TERMINATED:
		case PA_CONTEXT_FAILED:
			/* the connection was closed meanwhile; delete
			   it, and pulse_output_wait_connection() will
			   reopen it */
			DeleteContext();
			break;

		case PA_CONTEXT_READY:
		case PA_CONTEXT_CONNECTING:
		case PA_CONTEXT_AUTHORIZING:
		case PA_CONTEXT_SETTING_NAME:
			break;
		}
	}

	if (!WaitConnection(error)) {
		pa_threaded_mainloop_unlock(mainloop);
		return false;
	}

	/* MPD doesn't support the other pulseaudio sample formats, so
	   we just force MPD to send us everything as 16 bit */
	audio_format.format = SampleFormat::S16;

	pa_sample_spec ss;
	ss.format = PA_SAMPLE_S16NE;
	ss.rate = audio_format.sample_rate;
	ss.channels = audio_format.channels;

	/* create a stream .. */

	if (!SetupStream(ss, error)) {
		pa_threaded_mainloop_unlock(mainloop);
		return false;
	}

	/* .. and connect it (asynchronously) */

	if (pa_stream_connect_playback(stream, sink,
				       nullptr, pa_stream_flags_t(0),
				       nullptr, nullptr) < 0) {
		DeleteStream();

		SetPulseError(error, context,
			      "pa_stream_connect_playback() has failed");
		pa_threaded_mainloop_unlock(mainloop);
		return false;
	}

	pa_threaded_mainloop_unlock(mainloop);
	return true;
}

inline void
PulseOutput::Close()
{
	assert(mainloop != nullptr);

	pa_threaded_mainloop_lock(mainloop);

	if (pa_stream_get_state(stream) == PA_STREAM_READY) {
		pa_operation *o =
			pa_stream_drain(stream,
					pulse_output_stream_success_cb, this);
		if (o == nullptr) {
			LogPulseError(context,
				      "pa_stream_drain() has failed");
		} else
			pulse_wait_for_operation(mainloop, o);
	}

	DeleteStream();

	if (context != nullptr &&
	    pa_context_get_state(context) != PA_CONTEXT_READY)
		DeleteContext();

	pa_threaded_mainloop_unlock(mainloop);
}

bool
PulseOutput::WaitStream(Error &error)
{
	while (true) {
		switch (pa_stream_get_state(stream)) {
		case PA_STREAM_READY:
			return true;

		case PA_STREAM_FAILED:
		case PA_STREAM_TERMINATED:
		case PA_STREAM_UNCONNECTED:
			SetPulseError(error, context,
				      "failed to connect the stream");
			return false;

		case PA_STREAM_CREATING:
			pa_threaded_mainloop_wait(mainloop);
			break;
		}
	}
}

bool
PulseOutput::StreamPause(bool pause, Error &error)
{
	assert(mainloop != nullptr);
	assert(context != nullptr);
	assert(stream != nullptr);

	pa_operation *o = pa_stream_cork(stream, pause,
					 pulse_output_stream_success_cb, this);
	if (o == nullptr) {
		SetPulseError(error, context,
			      "pa_stream_cork() has failed");
		return false;
	}

	if (!pulse_wait_for_operation(mainloop, o)) {
		SetPulseError(error, context,
			      "pa_stream_cork() has failed");
		return false;
	}

	return true;
}

inline unsigned
PulseOutput::Delay()
{
	pa_threaded_mainloop_lock(mainloop);

	unsigned result = 0;
	if (base.pause && pa_stream_is_corked(stream) &&
	    pa_stream_get_state(stream) == PA_STREAM_READY)
		/* idle while paused */
		result = 1000;

	pa_threaded_mainloop_unlock(mainloop);

	return result;
}

inline size_t
PulseOutput::Play(const void *chunk, size_t size, Error &error)
{
	assert(mainloop != nullptr);
	assert(stream != nullptr);

	pa_threaded_mainloop_lock(mainloop);

	/* check if the stream is (already) connected */

	if (!WaitStream(error)) {
		pa_threaded_mainloop_unlock(mainloop);
		return 0;
	}

	assert(context != nullptr);

	/* unpause if previously paused */

	if (pa_stream_is_corked(stream) && !StreamPause(false, error)) {
		pa_threaded_mainloop_unlock(mainloop);
		return 0;
	}

	/* wait until the server allows us to write */

	while (writable == 0) {
		if (pa_stream_is_suspended(stream)) {
			pa_threaded_mainloop_unlock(mainloop);
			error.Set(pulse_domain, "suspended");
			return 0;
		}

		pa_threaded_mainloop_wait(mainloop);

		if (pa_stream_get_state(stream) != PA_STREAM_READY) {
			pa_threaded_mainloop_unlock(mainloop);
			error.Set(pulse_domain, "disconnected");
			return 0;
		}
	}

	/* now write */

	if (size > writable)
		/* don't send more than possible */
		size = writable;

	writable -= size;

	int result = pa_stream_write(stream, chunk, size, nullptr,
				     0, PA_SEEK_RELATIVE);
	pa_threaded_mainloop_unlock(mainloop);
	if (result < 0) {
		SetPulseError(error, context, "pa_stream_write() failed");
		return 0;
	}

	return size;
}

inline void
PulseOutput::Cancel()
{
	assert(mainloop != nullptr);
	assert(stream != nullptr);

	pa_threaded_mainloop_lock(mainloop);

	if (pa_stream_get_state(stream) != PA_STREAM_READY) {
		/* no need to flush when the stream isn't connected
		   yet */
		pa_threaded_mainloop_unlock(mainloop);
		return;
	}

	assert(context != nullptr);

	pa_operation *o = pa_stream_flush(stream,
					  pulse_output_stream_success_cb,
					  this);
	if (o == nullptr) {
		LogPulseError(context, "pa_stream_flush() has failed");
		pa_threaded_mainloop_unlock(mainloop);
		return;
	}

	pulse_wait_for_operation(mainloop, o);
	pa_threaded_mainloop_unlock(mainloop);
}

inline bool
PulseOutput::Pause()
{
	assert(mainloop != nullptr);
	assert(stream != nullptr);

	pa_threaded_mainloop_lock(mainloop);

	/* check if the stream is (already/still) connected */

	Error error;
	if (!WaitStream(error)) {
		pa_threaded_mainloop_unlock(mainloop);
		LogError(error);
		return false;
	}

	assert(context != nullptr);

	/* cork the stream */

	if (!pa_stream_is_corked(stream) && !StreamPause(true, error)) {
		pa_threaded_mainloop_unlock(mainloop);
		LogError(error);
		return false;
	}

	pa_threaded_mainloop_unlock(mainloop);
	return true;
}

inline bool
PulseOutput::TestDefaultDevice()
{
	const config_param empty;
	PulseOutput *po = PulseOutput::Create(empty, IgnoreError());
	if (po == nullptr)
		return false;

	bool success = po->WaitConnection(IgnoreError());
	delete po;
	return success;
}

static bool
pulse_output_test_default_device(void)
{
	return PulseOutput::TestDefaultDevice();
}

typedef AudioOutputWrapper<PulseOutput> Wrapper;

const struct AudioOutputPlugin pulse_output_plugin = {
	"pulse",
	pulse_output_test_default_device,
	&Wrapper::Init,
	&Wrapper::Finish,
	&Wrapper::Enable,
	&Wrapper::Disable,
	&Wrapper::Open,
	&Wrapper::Close,
	&Wrapper::Delay,
	nullptr,
	&Wrapper::Play,
	nullptr,
	&Wrapper::Cancel,
	&Wrapper::Pause,

	&pulse_mixer_plugin,
};
