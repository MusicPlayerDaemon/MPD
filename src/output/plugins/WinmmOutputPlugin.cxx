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
#include "WinmmOutputPlugin.hxx"
#include "../OutputAPI.hxx"
#include "pcm/PcmBuffer.hxx"
#include "mixer/MixerList.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "util/Macros.hxx"

#include <stdlib.h>
#include <string.h>

struct WinmmBuffer {
	PcmBuffer buffer;

	WAVEHDR hdr;
};

struct WinmmOutput {
	AudioOutput base;

	UINT device_id;
	HWAVEOUT handle;

	/**
	 * This event is triggered by Windows when a buffer is
	 * finished.
	 */
	HANDLE event;

	WinmmBuffer buffers[8];
	unsigned next_buffer;

	WinmmOutput()
		:base(winmm_output_plugin) {}
};

static constexpr Domain winmm_output_domain("winmm_output");

HWAVEOUT
winmm_output_get_handle(WinmmOutput &output)
{
	return output.handle;
}

static bool
winmm_output_test_default_device(void)
{
	return waveOutGetNumDevs() > 0;
}

static bool
get_device_id(const char *device_name, UINT *device_id, Error &error)
{
	/* if device is not specified use wave mapper */
	if (device_name == nullptr) {
		*device_id = WAVE_MAPPER;
		return true;
	}

	UINT numdevs = waveOutGetNumDevs();

	/* check for device id */
	char *endptr;
	UINT id = strtoul(device_name, &endptr, 0);
	if (endptr > device_name && *endptr == 0) {
		if (id >= numdevs)
			goto fail;
		*device_id = id;
		return true;
	}

	/* check for device name */
	for (UINT i = 0; i < numdevs; i++) {
		WAVEOUTCAPS caps;
		MMRESULT result = waveOutGetDevCaps(i, &caps, sizeof(caps));
		if (result != MMSYSERR_NOERROR)
			continue;
		/* szPname is only 32 chars long, so it is often truncated.
		   Use partial match to work around this. */
		if (strstr(device_name, caps.szPname) == device_name) {
			*device_id = i;
			return true;
		}
	}

fail:
	error.Format(winmm_output_domain,
		     "device \"%s\" is not found", device_name);
	return false;
}

static AudioOutput *
winmm_output_init(const config_param &param, Error &error)
{
	WinmmOutput *wo = new WinmmOutput();
	if (!wo->base.Configure(param, error)) {
		delete wo;
		return nullptr;
	}

	const char *device = param.GetBlockValue("device");
	if (!get_device_id(device, &wo->device_id, error)) {
		delete wo;
		return nullptr;
	}

	return &wo->base;
}

static void
winmm_output_finish(AudioOutput *ao)
{
	WinmmOutput *wo = (WinmmOutput *)ao;

	delete wo;
}

static bool
winmm_output_open(AudioOutput *ao, AudioFormat &audio_format,
		  Error &error)
{
	WinmmOutput *wo = (WinmmOutput *)ao;

	wo->event = CreateEvent(nullptr, false, false, nullptr);
	if (wo->event == nullptr) {
		error.Set(winmm_output_domain, "CreateEvent() failed");
		return false;
	}

	switch (audio_format.format) {
	case SampleFormat::S8:
	case SampleFormat::S16:
		break;

	case SampleFormat::S24_P32:
	case SampleFormat::S32:
	case SampleFormat::FLOAT:
	case SampleFormat::DSD:
	case SampleFormat::UNDEFINED:
		/* we havn't tested formats other than S16 */
		audio_format.format = SampleFormat::S16;
		break;
	}

	if (audio_format.channels > 2)
		/* same here: more than stereo was not tested */
		audio_format.channels = 2;

	WAVEFORMATEX format;
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = audio_format.channels;
	format.nSamplesPerSec = audio_format.sample_rate;
	format.nBlockAlign = audio_format.GetFrameSize();
	format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
	format.wBitsPerSample = audio_format.GetSampleSize() * 8;
	format.cbSize = 0;

	MMRESULT result = waveOutOpen(&wo->handle, wo->device_id, &format,
				      (DWORD_PTR)wo->event, 0, CALLBACK_EVENT);
	if (result != MMSYSERR_NOERROR) {
		CloseHandle(wo->event);
		error.Set(winmm_output_domain, "waveOutOpen() failed");
		return false;
	}

	for (unsigned i = 0; i < ARRAY_SIZE(wo->buffers); ++i) {
		memset(&wo->buffers[i].hdr, 0, sizeof(wo->buffers[i].hdr));
	}

	wo->next_buffer = 0;

	return true;
}

static void
winmm_output_close(AudioOutput *ao)
{
	WinmmOutput *wo = (WinmmOutput *)ao;

	for (unsigned i = 0; i < ARRAY_SIZE(wo->buffers); ++i)
		wo->buffers[i].buffer.Clear();

	waveOutClose(wo->handle);

	CloseHandle(wo->event);
}

/**
 * Copy data into a buffer, and prepare the wave header.
 */
static bool
winmm_set_buffer(WinmmOutput *wo, WinmmBuffer *buffer,
		 const void *data, size_t size,
		 Error &error)
{
	void *dest = buffer->buffer.Get(size);
	assert(dest != nullptr);

	memcpy(dest, data, size);

	memset(&buffer->hdr, 0, sizeof(buffer->hdr));
	buffer->hdr.lpData = (LPSTR)dest;
	buffer->hdr.dwBufferLength = size;

	MMRESULT result = waveOutPrepareHeader(wo->handle, &buffer->hdr,
					       sizeof(buffer->hdr));
	if (result != MMSYSERR_NOERROR) {
		error.Set(winmm_output_domain, result,
			  "waveOutPrepareHeader() failed");
		return false;
	}

	return true;
}

/**
 * Wait until the buffer is finished.
 */
static bool
winmm_drain_buffer(WinmmOutput *wo, WinmmBuffer *buffer,
		   Error &error)
{
	if ((buffer->hdr.dwFlags & WHDR_DONE) == WHDR_DONE)
		/* already finished */
		return true;

	while (true) {
		MMRESULT result = waveOutUnprepareHeader(wo->handle,
							 &buffer->hdr,
							 sizeof(buffer->hdr));
		if (result == MMSYSERR_NOERROR)
			return true;
		else if (result != WAVERR_STILLPLAYING) {
			error.Set(winmm_output_domain, result,
				  "waveOutUnprepareHeader() failed");
			return false;
		}

		/* wait some more */
		WaitForSingleObject(wo->event, INFINITE);
	}
}

static size_t
winmm_output_play(AudioOutput *ao, const void *chunk, size_t size, Error &error)
{
	WinmmOutput *wo = (WinmmOutput *)ao;

	/* get the next buffer from the ring and prepare it */
	WinmmBuffer *buffer = &wo->buffers[wo->next_buffer];
	if (!winmm_drain_buffer(wo, buffer, error) ||
	    !winmm_set_buffer(wo, buffer, chunk, size, error))
		return 0;

	/* enqueue the buffer */
	MMRESULT result = waveOutWrite(wo->handle, &buffer->hdr,
				       sizeof(buffer->hdr));
	if (result != MMSYSERR_NOERROR) {
		waveOutUnprepareHeader(wo->handle, &buffer->hdr,
				       sizeof(buffer->hdr));
		error.Set(winmm_output_domain, result,
			  "waveOutWrite() failed");
		return 0;
	}

	/* mark our buffer as "used" */
	wo->next_buffer = (wo->next_buffer + 1) %
		ARRAY_SIZE(wo->buffers);

	return size;
}

static bool
winmm_drain_all_buffers(WinmmOutput *wo, Error &error)
{
	for (unsigned i = wo->next_buffer; i < ARRAY_SIZE(wo->buffers); ++i)
		if (!winmm_drain_buffer(wo, &wo->buffers[i], error))
			return false;

	for (unsigned i = 0; i < wo->next_buffer; ++i)
		if (!winmm_drain_buffer(wo, &wo->buffers[i], error))
			return false;

	return true;
}

static void
winmm_stop(WinmmOutput *wo)
{
	waveOutReset(wo->handle);

	for (unsigned i = 0; i < ARRAY_SIZE(wo->buffers); ++i) {
		WinmmBuffer *buffer = &wo->buffers[i];
		waveOutUnprepareHeader(wo->handle, &buffer->hdr,
				       sizeof(buffer->hdr));
	}
}

static void
winmm_output_drain(AudioOutput *ao)
{
	WinmmOutput *wo = (WinmmOutput *)ao;

	if (!winmm_drain_all_buffers(wo, IgnoreError()))
		winmm_stop(wo);
}

static void
winmm_output_cancel(AudioOutput *ao)
{
	WinmmOutput *wo = (WinmmOutput *)ao;

	winmm_stop(wo);
}

const struct AudioOutputPlugin winmm_output_plugin = {
	"winmm",
	winmm_output_test_default_device,
	winmm_output_init,
	winmm_output_finish,
	nullptr,
	nullptr,
	winmm_output_open,
	winmm_output_close,
	nullptr,
	nullptr,
	winmm_output_play,
	winmm_output_drain,
	winmm_output_cancel,
	nullptr,
	&winmm_mixer_plugin,
};
