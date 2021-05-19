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

#include "WinmmOutputPlugin.hxx"
#include "../OutputAPI.hxx"
#include "pcm/Buffer.hxx"
#include "mixer/MixerList.hxx"
#include "fs/AllocatedPath.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringCompare.hxx"

#include <array>
#include <iterator>

#include <handleapi.h>
#include <synchapi.h>
#include <winbase.h> // for INFINITE

#include <stdlib.h>
#include <string.h>

struct WinmmBuffer {
	PcmBuffer buffer;

	WAVEHDR hdr;
};

class WinmmOutput final : AudioOutput {
	const UINT device_id;
	HWAVEOUT handle;

	/**
	 * This event is triggered by Windows when a buffer is
	 * finished.
	 */
	HANDLE event;

	std::array<WinmmBuffer, 8> buffers;
	unsigned next_buffer;

public:
	WinmmOutput(const ConfigBlock &block);

	HWAVEOUT GetHandle() {
		return handle;
	}

	static AudioOutput *Create(EventLoop &, const ConfigBlock &block) {
		return new WinmmOutput(block);
	}

private:
	void Open(AudioFormat &audio_format) override;
	void Close() noexcept override;

	size_t Play(const void *chunk, size_t size) override;
	void Drain() override;
	void Cancel() noexcept override;

private:
	/**
	 * Wait until the buffer is finished.
	 */
	void DrainBuffer(WinmmBuffer &buffer);

	void DrainAllBuffers();

	void Stop() noexcept;

};

static std::runtime_error
MakeWaveOutError(MMRESULT result, const char *prefix)
{
	char buffer[256];
	if (waveOutGetErrorTextA(result, buffer,
				 std::size(buffer)) == MMSYSERR_NOERROR)
		return FormatRuntimeError("%s: %s", prefix, buffer);
	else
		return std::runtime_error(prefix);
}

HWAVEOUT
winmm_output_get_handle(WinmmOutput &output)
{
	return output.GetHandle();
}

static bool
winmm_output_test_default_device(void)
{
	return waveOutGetNumDevs() > 0;
}

static UINT
get_device_id(const char *device_name)
{
	/* if device is not specified use wave mapper */
	if (device_name == nullptr)
		return WAVE_MAPPER;

	UINT numdevs = waveOutGetNumDevs();

	/* check for device id */
	char *endptr;
	UINT id = strtoul(device_name, &endptr, 0);
	if (endptr > device_name && *endptr == 0) {
		if (id >= numdevs)
			throw FormatRuntimeError("device \"%s\" is not found",
						 device_name);

		return id;
	}

	/* check for device name */
	const AllocatedPath device_name_fs =
		AllocatedPath::FromUTF8Throw(device_name);

	for (UINT i = 0; i < numdevs; i++) {
		WAVEOUTCAPS caps;
		MMRESULT result = waveOutGetDevCaps(i, &caps, sizeof(caps));
		if (result != MMSYSERR_NOERROR)
			continue;
		/* szPname is only 32 chars long, so it is often truncated.
		   Use partial match to work around this. */
		if (StringStartsWith(device_name_fs.c_str(), caps.szPname))
			return i;
	}

	throw FormatRuntimeError("device \"%s\" is not found", device_name);
}

WinmmOutput::WinmmOutput(const ConfigBlock &block)
	:AudioOutput(0),
	 device_id(get_device_id(block.GetBlockValue("device")))
{
}

void
WinmmOutput::Open(AudioFormat &audio_format)
{
	event = CreateEvent(nullptr, false, false, nullptr);
	if (event == nullptr)
		throw std::runtime_error("CreateEvent() failed");

	switch (audio_format.format) {
	case SampleFormat::S16:
		break;

	case SampleFormat::S8:
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

	MMRESULT result = waveOutOpen(&handle, device_id, &format,
				      (DWORD_PTR)event, 0, CALLBACK_EVENT);
	if (result != MMSYSERR_NOERROR) {
		CloseHandle(event);
		throw MakeWaveOutError(result, "waveOutOpen() failed");
	}

	for (auto &i : buffers)
		memset(&i.hdr, 0, sizeof(i.hdr));

	next_buffer = 0;
}

void
WinmmOutput::Close() noexcept
{
	for (auto &i : buffers)
		i.buffer.Clear();

	waveOutClose(handle);

	CloseHandle(event);
}

/**
 * Copy data into a buffer, and prepare the wave header.
 */
static void
winmm_set_buffer(HWAVEOUT handle, WinmmBuffer *buffer,
		 const void *data, size_t size)
{
	void *dest = buffer->buffer.Get(size);
	assert(dest != nullptr);

	memcpy(dest, data, size);

	memset(&buffer->hdr, 0, sizeof(buffer->hdr));
	buffer->hdr.lpData = (LPSTR)dest;
	buffer->hdr.dwBufferLength = size;

	MMRESULT result = waveOutPrepareHeader(handle, &buffer->hdr,
					       sizeof(buffer->hdr));
	if (result != MMSYSERR_NOERROR)
		throw MakeWaveOutError(result,
				       "waveOutPrepareHeader() failed");
}

void
WinmmOutput::DrainBuffer(WinmmBuffer &buffer)
{
	if ((buffer.hdr.dwFlags & WHDR_DONE) == WHDR_DONE)
		/* already finished */
		return;

	while (true) {
		MMRESULT result = waveOutUnprepareHeader(handle,
							 &buffer.hdr,
							 sizeof(buffer.hdr));
		if (result == MMSYSERR_NOERROR)
			return;
		else if (result != WAVERR_STILLPLAYING)
			throw MakeWaveOutError(result,
					       "waveOutUnprepareHeader() failed");

		/* wait some more */
		WaitForSingleObject(event, INFINITE);
	}
}

size_t
WinmmOutput::Play(const void *chunk, size_t size)
{
	/* get the next buffer from the ring and prepare it */
	WinmmBuffer *buffer = &buffers[next_buffer];
	DrainBuffer(*buffer);
	winmm_set_buffer(handle, buffer, chunk, size);

	/* enqueue the buffer */
	MMRESULT result = waveOutWrite(handle, &buffer->hdr,
				       sizeof(buffer->hdr));
	if (result != MMSYSERR_NOERROR) {
		waveOutUnprepareHeader(handle, &buffer->hdr,
				       sizeof(buffer->hdr));
		throw MakeWaveOutError(result, "waveOutWrite() failed");
	}

	/* mark our buffer as "used" */
	next_buffer = (next_buffer + 1) % buffers.size();

	return size;
}

void
WinmmOutput::DrainAllBuffers()
{
	for (unsigned i = next_buffer; i < buffers.size(); ++i)
		DrainBuffer(buffers[i]);

	for (unsigned i = 0; i < next_buffer; ++i)
		DrainBuffer(buffers[i]);
}

void
WinmmOutput::Stop() noexcept
{
	waveOutReset(handle);

	for (auto &i : buffers)
		waveOutUnprepareHeader(handle, &i.hdr, sizeof(i.hdr));
}

void
WinmmOutput::Drain()
{
	try {
		DrainAllBuffers();
	} catch (...) {
		Stop();
		throw;
	}
}

void
WinmmOutput::Cancel() noexcept
{
	Stop();
}

const struct AudioOutputPlugin winmm_output_plugin = {
	"winmm",
	winmm_output_test_default_device,
	WinmmOutput::Create,
	&winmm_mixer_plugin,
};
