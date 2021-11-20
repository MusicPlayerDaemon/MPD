/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "SlesOutputPlugin.hxx"
#include "Object.hxx"
#include "Engine.hxx"
#include "Play.hxx"
#include "AndroidSimpleBufferQueue.hxx"
#include "../../OutputAPI.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "util/Domain.hxx"
#include "util/ByteOrder.hxx"
#include "mixer/MixerList.hxx"
#include "Log.hxx"

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <cassert>
#include <iterator>
#include <stdexcept>

class SlesOutput final : AudioOutput  {
	static constexpr unsigned N_BUFFERS = 3;
	static constexpr size_t BUFFER_SIZE = 65536;

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

	SlesOutput():AudioOutput(FLAG_PAUSE) {}

public:
	static AudioOutput *Create(EventLoop &, const ConfigBlock &) {
		return new SlesOutput();
	}

private:
	void Open(AudioFormat &audio_format) override;
	void Close() noexcept override;

	std::chrono::steady_clock::duration Delay() const noexcept override {
		return pause && !cancel
			? std::chrono::milliseconds(100)
			: std::chrono::steady_clock::duration::zero();
	}

	size_t Play(const void *chunk, size_t size) override;

	void Drain() override;
	void Cancel() noexcept override;
	bool Pause() override;

private:
	void PlayedCallback();

	/**
	 * OpenSL/ES callback which gets invoked when a buffer has
	 * been consumed.  It synthesises and enqueues the next
	 * buffer.
	 */
	static void PlayedCallback([[maybe_unused]] SLAndroidSimpleBufferQueueItf caller,
				   void *pContext)
	{
		SlesOutput &sles = *(SlesOutput *)pContext;
		sles.PlayedCallback();
	}
};

static constexpr Domain sles_domain("sles");

void
SlesOutput::Open(AudioFormat &audio_format)
{
	SLresult result;
	SLObjectItf _object;

	result = slCreateEngine(&_object, 0, nullptr, 0,
				nullptr, nullptr);
	if (result != SL_RESULT_SUCCESS)
		throw std::runtime_error("slCreateEngine() failed");

	engine_object = SLES::Object(_object);

	result = engine_object.Realize(false);
	if (result != SL_RESULT_SUCCESS) {
		engine_object.Destroy();
		throw std::runtime_error("Engine.Realize() failed");
	}

	SLEngineItf _engine;
	result = engine_object.GetInterface(SL_IID_ENGINE, &_engine);
	if (result != SL_RESULT_SUCCESS) {
		engine_object.Destroy();
		throw std::runtime_error("Engine.GetInterface(IID_ENGINE) failed");
	}

	SLES::Engine engine(_engine);

	result = engine.CreateOutputMix(&_object, 0, nullptr, nullptr);
	if (result != SL_RESULT_SUCCESS) {
		engine_object.Destroy();
		throw std::runtime_error("Engine.CreateOutputMix() failed");
	}

	mix_object = SLES::Object(_object);

	result = mix_object.Realize(false);
	if (result != SL_RESULT_SUCCESS) {
		mix_object.Destroy();
		engine_object.Destroy();
		throw std::runtime_error("Mix.Realize() failed");
	}

	SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
		SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
		N_BUFFERS,
	};

	if (audio_format.channels > 2)
		audio_format.channels = 1;

	SLAndroidDataFormat_PCM_EX format_pcm;
	format_pcm.formatType = SL_DATAFORMAT_PCM;
	format_pcm.numChannels = audio_format.channels;
	/* from the Android NDK docs: "Note that the field samplesPerSec is
	   actually in units of milliHz, despite the misleading name." */
	format_pcm.sampleRate = audio_format.sample_rate * 1000u;
	format_pcm.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
	format_pcm.containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
	format_pcm.channelMask = audio_format.channels == 1
		? SL_SPEAKER_FRONT_CENTER
		: SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
	format_pcm.endianness = IsLittleEndian()
		? SL_BYTEORDER_LITTLEENDIAN
		: SL_BYTEORDER_BIGENDIAN;
	format_pcm.representation = SL_ANDROID_PCM_REPRESENTATION_SIGNED_INT;

	switch (audio_format.format) {
		/* note: Android doesn't support
		   SL_PCMSAMPLEFORMAT_FIXED_24 and
		   SL_PCMSAMPLEFORMAT_FIXED_32, so let's not bother
		   implement it here; SL_PCMSAMPLEFORMAT_FIXED_8
		   appears to be unsigned, so not usable for us (and
		   converting S8 to U8 is not worth the trouble) */

	case SampleFormat::S16:
		/* bitsPerSample and containerSize already set for 16
		   bit */
		break;

	case SampleFormat::FLOAT:
		/* Android has an OpenSLES extension for floating
		   point samples:
		   https://developer.android.com/ndk/guides/audio/opensl/android-extensions */
		format_pcm.formatType = SL_ANDROID_DATAFORMAT_PCM_EX;
		format_pcm.bitsPerSample = format_pcm.containerSize =
			SL_PCMSAMPLEFORMAT_FIXED_32;
		format_pcm.representation = SL_ANDROID_PCM_REPRESENTATION_FLOAT;
		break;

	default:
		/* fall back to 16 bit */
		audio_format.format = SampleFormat::S16;
		break;
	}

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
					  std::size(ids2), ids2, req2);
	if (result != SL_RESULT_SUCCESS) {
		mix_object.Destroy();
		engine_object.Destroy();
		throw std::runtime_error("Engine.CreateAudioPlayer() failed");
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

		/* MPD doesn't care much about latency, so let's
		   configure power saving mode */
		SLuint32 performance_mode = SL_ANDROID_PERFORMANCE_POWER_SAVING;
		(*android_config)->SetConfiguration(android_config,
						    SL_ANDROID_KEY_PERFORMANCE_MODE,
						    &performance_mode,
						    sizeof(performance_mode));
	}

	result = play_object.Realize(false);

	if (result != SL_RESULT_SUCCESS) {
		play_object.Destroy();
		mix_object.Destroy();
		engine_object.Destroy();
		throw std::runtime_error("Play.Realize() failed");
	}

	SLPlayItf _play;
	result = play_object.GetInterface(SL_IID_PLAY, &_play);
	if (result != SL_RESULT_SUCCESS) {
		play_object.Destroy();
		mix_object.Destroy();
		engine_object.Destroy();
		throw std::runtime_error("Play.GetInterface(IID_PLAY) failed");
	}

	play = SLES::Play(_play);

	SLAndroidSimpleBufferQueueItf _queue;
	result = play_object.GetInterface(SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
					  &_queue);
	if (result != SL_RESULT_SUCCESS) {
		play_object.Destroy();
		mix_object.Destroy();
		engine_object.Destroy();
		throw std::runtime_error("Play.GetInterface(IID_ANDROIDSIMPLEBUFFERQUEUE) failed");
	}

	queue = SLES::AndroidSimpleBufferQueue(_queue);
	result = queue.RegisterCallback(PlayedCallback, (void *)this);
	if (result != SL_RESULT_SUCCESS) {
		play_object.Destroy();
		mix_object.Destroy();
		engine_object.Destroy();
		throw std::runtime_error("Play.RegisterCallback() failed");
	}

	result = play.SetPlayState(SL_PLAYSTATE_PLAYING);
	if (result != SL_RESULT_SUCCESS) {
		play_object.Destroy();
		mix_object.Destroy();
		engine_object.Destroy();
		throw std::runtime_error("Play.SetPlayState(PLAYING) failed");
	}

	pause = cancel = false;
	n_queued = 0;
	next = 0;
	filled = 0;
}

void
SlesOutput::Close() noexcept
{
	play.SetPlayState(SL_PLAYSTATE_STOPPED);
	play_object.Destroy();
	mix_object.Destroy();
	engine_object.Destroy();
}

size_t
SlesOutput::Play(const void *chunk, size_t size)
{
	cancel = false;

	if (pause) {
		SLresult result = play.SetPlayState(SL_PLAYSTATE_PLAYING);
		if (result != SL_RESULT_SUCCESS)
			throw std::runtime_error("Play.SetPlayState(PLAYING) failed");

		pause = false;
	}

	std::unique_lock<Mutex> lock(mutex);

	assert(filled < BUFFER_SIZE);

	cond.wait(lock, [this]{
		bool ret = n_queued != N_BUFFERS;
		assert(ret || filled == 0);
		return ret;
	});

	size_t nbytes = std::min(BUFFER_SIZE - filled, size);
	memcpy(buffers[next] + filled, chunk, nbytes);
	filled += nbytes;
	if (filled < BUFFER_SIZE)
		return nbytes;

	SLresult result = queue.Enqueue(buffers[next], BUFFER_SIZE);
	if (result != SL_RESULT_SUCCESS)
		throw std::runtime_error("AndroidSimpleBufferQueue.Enqueue() failed");

	++n_queued;
	next = (next + 1) % N_BUFFERS;
	filled = 0;

	return nbytes;
}

void
SlesOutput::Drain()
{
	std::unique_lock<Mutex> lock(mutex);

	assert(filled < BUFFER_SIZE);

	cond.wait(lock, [this]{ return n_queued == 0; });
}

void
SlesOutput::Cancel() noexcept
{
	pause = true;
	cancel = true;

	SLresult result = play.SetPlayState(SL_PLAYSTATE_PAUSED);
	if (result != SL_RESULT_SUCCESS)
		LogError(sles_domain,  "Play.SetPlayState(PAUSED) failed");

	result = queue.Clear();
	if (result != SL_RESULT_SUCCESS)
		LogWarning(sles_domain,
			   "AndroidSimpleBufferQueue.Clear() failed");

	const std::scoped_lock<Mutex> protect(mutex);
	n_queued = 0;
	filled = 0;
}

bool
SlesOutput::Pause()
{
	cancel = false;

	if (pause)
		return true;

	pause = true;

	SLresult result = play.SetPlayState(SL_PLAYSTATE_PAUSED);
	if (result != SL_RESULT_SUCCESS)
		throw std::runtime_error("Play.SetPlayState(PAUSED) failed");

	return true;
}

inline void
SlesOutput::PlayedCallback()
{
	const std::scoped_lock<Mutex> protect(mutex);
	assert(n_queued > 0);
	--n_queued;
	cond.notify_one();
}

static bool
sles_test_default_device()
{
	/* this is the default output plugin on Android, and it should
	   be available in any case */
	return true;
}

const struct AudioOutputPlugin sles_output_plugin = {
	"sles",
	sles_test_default_device,
	SlesOutput::Create,
	&android_mixer_plugin,
};
