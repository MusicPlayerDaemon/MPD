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
#include "SlesOutputPlugin.hxx"
#include "Object.hxx"
#include "Engine.hxx"
#include "Play.hxx"
#include "AndroidSimpleBufferQueue.hxx"
#include "../../OutputAPI.hxx"
#include "util/Macros.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "system/ByteOrder.hxx"
#include "Log.hxx"

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

class SlesOutput {
	static constexpr unsigned N_BUFFERS = 3;
	static constexpr size_t BUFFER_SIZE = 65536;

	AudioOutput base;

	SLES::Object engine_object, mix_object, play_object;
	SLES::Play play;
	SLES::AndroidSimpleBufferQueue queue;

	/**
	 * This mutex protects the attributes "next" and "filled".  It
	 * is only needed while playback is launched, when the initial
	 * buffers are being enqueued in the caller thread, while
	 * another thread may invoke the registered callback.
	 */
	Mutex mutex;

	Cond cond;

	bool pause, cancel;

	/**
	 * The number of buffers queued to OpenSLES.
	 */
	unsigned n_queued;

	/**
	 * The index of the next buffer to be enqueued.
	 */
	unsigned next;

	/**
	 * Does the "next" buffer already contain synthesised samples?
	 * This can happen when PCMSynthesiser::Synthesise() has been
	 * called, but the OpenSL/ES buffer queue was full.  The
	 * buffer will then be postponed.
	 */
	unsigned filled;

	/**
	 * An array of buffers.  It's one more than being managed by
	 * OpenSL/ES, and the one not enqueued (see attribute #next)
	 * will be written to.
	 */
	uint8_t buffers[N_BUFFERS][BUFFER_SIZE];

public:
	SlesOutput()
		:base(sles_output_plugin) {}

	operator AudioOutput *() {
		return &base;
	}

	bool Initialize(const config_param &param, Error &error) {
		return base.Configure(param, error);
	}

	bool Configure(const config_param &param, Error &error);

	bool Open(AudioFormat &audio_format, Error &error);
	void Close();

	unsigned Delay() {
		return pause && !cancel ? 100 : 0;
	}

	size_t Play(const void *chunk, size_t size, Error &error);

	void Drain();
	void Cancel();
	bool Pause();

private:
	void PlayedCallback();

	/**
	 * OpenSL/ES callback which gets invoked when a buffer has
	 * been consumed.  It synthesises and enqueues the next
	 * buffer.
	 */
	static void PlayedCallback(gcc_unused SLAndroidSimpleBufferQueueItf caller,
				   void *pContext)
	{
		SlesOutput &sles = *(SlesOutput *)pContext;
		sles.PlayedCallback();
	}
};

static constexpr Domain sles_domain("sles");

inline bool
SlesOutput::Configure(const config_param &, Error &)
{
	return true;
}

inline bool
SlesOutput::Open(AudioFormat &audio_format, Error &error)
{
	SLresult result;
	SLObjectItf _object;

	result = slCreateEngine(&_object, 0, nullptr, 0,
				nullptr, nullptr);
	if (result != SL_RESULT_SUCCESS) {
		error.Set(sles_domain, int(result), "slCreateEngine() failed");
		return false;
	}

	engine_object = SLES::Object(_object);

	result = engine_object.Realize(false);
	if (result != SL_RESULT_SUCCESS) {
		error.Set(sles_domain, int(result), "Engine.Realize() failed");
		engine_object.Destroy();
		return false;
	}

	SLEngineItf _engine;
	result = engine_object.GetInterface(SL_IID_ENGINE, &_engine);
	if (result != SL_RESULT_SUCCESS) {
		error.Set(sles_domain, int(result),
			  "Engine.GetInterface(IID_ENGINE) failed");
		engine_object.Destroy();
		return false;
	}

	SLES::Engine engine(_engine);

	result = engine.CreateOutputMix(&_object, 0, nullptr, nullptr);
	if (result != SL_RESULT_SUCCESS) {
		error.Set(sles_domain, int(result),
			  "Engine.CreateOutputMix() failed");
		engine_object.Destroy();
		return false;
	}

	mix_object = SLES::Object(_object);

	result = mix_object.Realize(false);
	if (result != SL_RESULT_SUCCESS) {
		error.Set(sles_domain, int(result),
			  "Mix.Realize() failed");
		mix_object.Destroy();
		engine_object.Destroy();
		return false;
	}

	SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
		SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
		N_BUFFERS,
	};

	if (audio_format.channels > 2)
		audio_format.channels = 1;

	SLDataFormat_PCM format_pcm;
	format_pcm.formatType = SL_DATAFORMAT_PCM;
	format_pcm.numChannels = audio_format.channels;
	/* from the Android NDK docs: "Note that the field samplesPerSec is
	   actually in units of milliHz, despite the misleading name." */
	format_pcm.samplesPerSec = audio_format.sample_rate * 1000u;
	format_pcm.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
	format_pcm.containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
	format_pcm.channelMask = audio_format.channels == 1
		? SL_SPEAKER_FRONT_CENTER
		: SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
	format_pcm.endianness = IsLittleEndian()
		? SL_BYTEORDER_LITTLEENDIAN
		: SL_BYTEORDER_BIGENDIAN;

	SLDataSource audioSrc = { &loc_bufq, &format_pcm };

	SLDataLocator_OutputMix loc_outmix = {
		SL_DATALOCATOR_OUTPUTMIX,
		mix_object,
	};

	SLDataSink audioSnk = {
		&loc_outmix,
		nullptr,
	};

	const SLInterfaceID ids2[] = {
		SL_IID_PLAY,
		SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
		SL_IID_ANDROIDCONFIGURATION,
	};

	static constexpr SLboolean req2[] = {
		SL_BOOLEAN_TRUE,
		SL_BOOLEAN_TRUE,
		SL_BOOLEAN_TRUE,
	};

	result = engine.CreateAudioPlayer(&_object, &audioSrc, &audioSnk,
					  ARRAY_SIZE(ids2), ids2, req2);
	if (result != SL_RESULT_SUCCESS) {
		error.Set(sles_domain, int(result),
			  "Engine.CreateAudioPlayer() failed");
		mix_object.Destroy();
		engine_object.Destroy();
		return false;
	}

	play_object = SLES::Object(_object);

	SLAndroidConfigurationItf android_config;
	if (play_object.GetInterface(SL_IID_ANDROIDCONFIGURATION,
				     &android_config) == SL_RESULT_SUCCESS) {
		SLint32 stream_type = SL_ANDROID_STREAM_MEDIA;
		(*android_config)->SetConfiguration(android_config,
						    SL_ANDROID_KEY_STREAM_TYPE,
						    &stream_type,
						    sizeof(stream_type));
	}

	result = play_object.Realize(false);

	if (result != SL_RESULT_SUCCESS) {
		error.Set(sles_domain, int(result),
			  "Play.Realize() failed");
		play_object.Destroy();
		mix_object.Destroy();
		engine_object.Destroy();
		return false;
	}

	SLPlayItf _play;
	result = play_object.GetInterface(SL_IID_PLAY, &_play);
	if (result != SL_RESULT_SUCCESS) {
		error.Set(sles_domain, int(result),
			  "Play.GetInterface(IID_PLAY) failed");
		play_object.Destroy();
		mix_object.Destroy();
		engine_object.Destroy();
		return false;
	}

	play = SLES::Play(_play);

	SLAndroidSimpleBufferQueueItf _queue;
	result = play_object.GetInterface(SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
					  &_queue);
	if (result != SL_RESULT_SUCCESS) {
		error.Set(sles_domain, int(result),
			  "Play.GetInterface(IID_ANDROIDSIMPLEBUFFERQUEUE) failed");
		play_object.Destroy();
		mix_object.Destroy();
		engine_object.Destroy();
		return false;
	}

	queue = SLES::AndroidSimpleBufferQueue(_queue);
	result = queue.RegisterCallback(PlayedCallback, (void *)this);
	if (result != SL_RESULT_SUCCESS) {
		error.Set(sles_domain, int(result),
			  "Play.RegisterCallback() failed");
		play_object.Destroy();
		mix_object.Destroy();
		engine_object.Destroy();
		return false;
	}

	result = play.SetPlayState(SL_PLAYSTATE_PLAYING);
	if (result != SL_RESULT_SUCCESS) {
		error.Set(sles_domain, int(result),
			  "Play.SetPlayState(PLAYING) failed");
		play_object.Destroy();
		mix_object.Destroy();
		engine_object.Destroy();
		return false;
	}

	pause = cancel = false;
	n_queued = 0;
	next = 0;
	filled = 0;

	// TODO: support other sample formats
	audio_format.format = SampleFormat::S16;

	return true;
}

inline void
SlesOutput::Close()
{
	play.SetPlayState(SL_PLAYSTATE_STOPPED);
	play_object.Destroy();
	mix_object.Destroy();
	engine_object.Destroy();
}

inline size_t
SlesOutput::Play(const void *chunk, size_t size, Error &error)
{
	cancel = false;

	if (pause) {
		SLresult result = play.SetPlayState(SL_PLAYSTATE_PLAYING);
		if (result != SL_RESULT_SUCCESS) {
			error.Set(sles_domain, int(result),
				  "Play.SetPlayState(PLAYING) failed");
			return false;
		}

		pause = false;
	}

	const ScopeLock protect(mutex);

	assert(filled < BUFFER_SIZE);

	while (n_queued == N_BUFFERS) {
		assert(filled == 0);
		cond.wait(mutex);
	}

	size_t nbytes = std::min(BUFFER_SIZE - filled, size);
	memcpy(buffers[next] + filled, chunk, nbytes);
	filled += nbytes;
	if (filled < BUFFER_SIZE)
		return nbytes;

	SLresult result = queue.Enqueue(buffers[next], BUFFER_SIZE);
	if (result != SL_RESULT_SUCCESS) {
		error.Set(sles_domain, int(result),
			  "AndroidSimpleBufferQueue.Enqueue() failed");
		return 0;
	}

	++n_queued;
	next = (next + 1) % N_BUFFERS;
	filled = 0;

	return nbytes;
}

inline void
SlesOutput::Drain()
{
	const ScopeLock protect(mutex);

	assert(filled < BUFFER_SIZE);

	while (n_queued > 0)
		cond.wait(mutex);
}

inline void
SlesOutput::Cancel()
{
	pause = true;
	cancel = true;

	SLresult result = play.SetPlayState(SL_PLAYSTATE_PAUSED);
	if (result != SL_RESULT_SUCCESS)
		FormatError(sles_domain,  "Play.SetPlayState(PAUSED) failed");

	result = queue.Clear();
	if (result != SL_RESULT_SUCCESS)
		FormatWarning(sles_domain,
			      "AndroidSimpleBufferQueue.Clear() failed");

	const ScopeLock protect(mutex);
	n_queued = 0;
	filled = 0;
}

inline bool
SlesOutput::Pause()
{
	cancel = false;

	if (pause)
		return true;

	pause = true;

	SLresult result = play.SetPlayState(SL_PLAYSTATE_PAUSED);
	if (result != SL_RESULT_SUCCESS) {
		FormatError(sles_domain, "Play.SetPlayState(PAUSED) failed");
		return false;
	}

	return true;
}

inline void
SlesOutput::PlayedCallback()
{
	const ScopeLock protect(mutex);
	assert(n_queued > 0);
	--n_queued;
	cond.signal();
}

static bool
sles_test_default_device()
{
	/* this is the default output plugin on Android, and it should
	   be available in any case */
	return true;
}

static AudioOutput *
sles_output_init(const config_param &param, Error &error)
{
	SlesOutput *sles = new SlesOutput();

	if (!sles->Initialize(param, error) ||
	    !sles->Configure(param, error)) {
		delete sles;
		return nullptr;
	}

	return *sles;
}

static void
sles_output_finish(AudioOutput *ao)
{
	SlesOutput *sles = (SlesOutput *)ao;

	delete sles;
}

static bool
sles_output_open(AudioOutput *ao, AudioFormat &audio_format, Error &error)
{
	SlesOutput &sles = *(SlesOutput *)ao;

	return sles.Open(audio_format, error);
}

static void
sles_output_close(AudioOutput *ao)
{
	SlesOutput &sles = *(SlesOutput *)ao;

	sles.Close();
}

static unsigned
sles_output_delay(AudioOutput *ao)
{
	SlesOutput &sles = *(SlesOutput *)ao;

	return sles.Delay();
}

static size_t
sles_output_play(AudioOutput *ao, const void *chunk, size_t size,
		 Error &error)
{
	SlesOutput &sles = *(SlesOutput *)ao;

	return sles.Play(chunk, size, error);
}

static void
sles_output_drain(AudioOutput *ao)
{
	SlesOutput &sles = *(SlesOutput *)ao;

	sles.Drain();
}

static void
sles_output_cancel(AudioOutput *ao)
{
	SlesOutput &sles = *(SlesOutput *)ao;

	sles.Cancel();
}

static bool
sles_output_pause(AudioOutput *ao)
{
	SlesOutput &sles = *(SlesOutput *)ao;

	return sles.Pause();
}

const struct AudioOutputPlugin sles_output_plugin = {
	"sles",
	sles_test_default_device,
	sles_output_init,
	sles_output_finish,
	nullptr,
	nullptr,
	sles_output_open,
	sles_output_close,
	sles_output_delay,
	nullptr,
	sles_output_play,
	sles_output_drain,
	sles_output_cancel,
	sles_output_pause,
	nullptr,
};
