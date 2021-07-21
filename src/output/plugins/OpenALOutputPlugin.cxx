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

#include "OpenALOutputPlugin.hxx"
#include "../OutputAPI.hxx"
#include "util/RuntimeError.hxx"

#include <unistd.h>

#ifndef __APPLE__
#include <AL/al.h>
#include <AL/alc.h>
#else
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
/* on macOS, OpenAL is deprecated, but since the user asked to enable
   this plugin, let's ignore the compiler warnings */
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

class OpenALOutput final : AudioOutput {
	/* should be enough for buffer size = 2048 */
	static constexpr unsigned NUM_BUFFERS = 16;

	const char *device_name;
	ALCdevice *device;
	ALCcontext *context;
	ALuint buffers[NUM_BUFFERS];
	unsigned filled;
	ALuint source;
	ALenum format;
	ALuint frequency;

	explicit OpenALOutput(const ConfigBlock &block);

public:
	static AudioOutput *Create(EventLoop &,
				   const ConfigBlock &block) {
		return new OpenALOutput(block);
	}

private:
	void Open(AudioFormat &audio_format) override;
	void Close() noexcept override;

	[[nodiscard]] gcc_pure
	std::chrono::steady_clock::duration Delay() const noexcept override {
		return filled < NUM_BUFFERS || HasProcessed()
			? std::chrono::steady_clock::duration::zero()
			/* we don't know exactly how long we must wait
			   for the next buffer to finish, so this is a
			   random guess: */
			: std::chrono::milliseconds(50);
	}

	size_t Play(const void *chunk, size_t size) override;

	void Cancel() noexcept override;

	[[nodiscard]] gcc_pure
	ALint GetSourceI(ALenum param) const noexcept {
		ALint value;
		alGetSourcei(source, param, &value);
		return value;
	}

	[[nodiscard]] gcc_pure
	bool HasProcessed() const noexcept {
		return GetSourceI(AL_BUFFERS_PROCESSED) > 0;
	}

	[[nodiscard]] gcc_pure
	bool IsPlaying() const noexcept {
		return GetSourceI(AL_SOURCE_STATE) == AL_PLAYING;
	}

	/**
	 * Throws on error.
	 */
	void SetupContext();
};

static ALenum
openal_audio_format(AudioFormat &audio_format)
{
	/* note: cannot map SampleFormat::S8 to AL_FORMAT_STEREO8 or
	   AL_FORMAT_MONO8 since OpenAL expects unsigned 8 bit
	   samples, while MPD uses signed samples */

	switch (audio_format.format) {
	case SampleFormat::S16:
		if (audio_format.channels == 2)
			return AL_FORMAT_STEREO16;
		if (audio_format.channels == 1)
			return AL_FORMAT_MONO16;

		/* fall back to mono */
		audio_format.channels = 1;
		return openal_audio_format(audio_format);

	default:
		/* fall back to 16 bit */
		audio_format.format = SampleFormat::S16;
		return openal_audio_format(audio_format);
	}
}

inline void
OpenALOutput::SetupContext()
{
	device = alcOpenDevice(device_name);
	if (device == nullptr)
		throw FormatRuntimeError("Error opening OpenAL device \"%s\"",
					 device_name);

	context = alcCreateContext(device, nullptr);
	if (context == nullptr) {
		alcCloseDevice(device);
		throw FormatRuntimeError("Error creating context for \"%s\"",
					 device_name);
	}
}

OpenALOutput::OpenALOutput(const ConfigBlock &block)
	:AudioOutput(0),
	 device_name(block.GetBlockValue("device"))
{
	if (device_name == nullptr)
		device_name = alcGetString(nullptr,
					   ALC_DEFAULT_DEVICE_SPECIFIER);
}

void
OpenALOutput::Open(AudioFormat &audio_format)
{
	format = openal_audio_format(audio_format);

	SetupContext();

	alcMakeContextCurrent(context);
	alGenBuffers(NUM_BUFFERS, buffers);

	if (alGetError() != AL_NO_ERROR)
		throw std::runtime_error("Failed to generate buffers");

	alGenSources(1, &source);

	if (alGetError() != AL_NO_ERROR) {
		alDeleteBuffers(NUM_BUFFERS, buffers);
		throw std::runtime_error("Failed to generate source");
	}

	filled = 0;
	frequency = audio_format.sample_rate;
}

void
OpenALOutput::Close() noexcept
{
	alcMakeContextCurrent(context);
	alDeleteSources(1, &source);
	alDeleteBuffers(NUM_BUFFERS, buffers);
	alcDestroyContext(context);
	alcCloseDevice(device);
}

size_t
OpenALOutput::Play(const void *chunk, size_t size)
{
	if (alcGetCurrentContext() != context)
		alcMakeContextCurrent(context);

	ALuint buffer;
	if (filled < NUM_BUFFERS) {
		/* fill all buffers */
		buffer = buffers[filled];
		filled++;
	} else {
		/* wait for processed buffer */
		while (!HasProcessed())
			usleep(10);

		alSourceUnqueueBuffers(source, 1, &buffer);
	}

	alBufferData(buffer, format, chunk, size, frequency);
	alSourceQueueBuffers(source, 1, &buffer);

	if (!IsPlaying())
		alSourcePlay(source);

	return size;
}

void
OpenALOutput::Cancel() noexcept
{
	filled = 0;
	alcMakeContextCurrent(context);
	alSourceStop(source);

	/* force-unqueue all buffers */
	alSourcei(source, AL_BUFFER, 0);
	filled = 0;
}

const struct AudioOutputPlugin openal_output_plugin = {
	"openal",
	nullptr,
	OpenALOutput::Create,
	nullptr,
};
