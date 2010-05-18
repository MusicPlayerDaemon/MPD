/*
 * Copyright (C) 2003-2010 The Music Player Daemon Project
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
#include "output_api.h"
#include "pcm_buffer.h"

#include <windows.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "win32_output"

struct win32_buffer {
	struct pcm_buffer buffer;

	WAVEHDR hdr;
};

struct win32_output {
	HWAVEOUT handle;

	/**
	 * This event is triggered by Windows when a buffer is
	 * finished.
	 */
	HANDLE event;

	struct win32_buffer buffers[8];
	unsigned next_buffer;
};

/**
 * The quark used for GError.domain.
 */
static inline GQuark
win32_output_quark(void)
{
	return g_quark_from_static_string("win32_output");
}

static bool
win32_output_test_default_device(void)
{
	/* we assume that Wave is always available */
	return true;
}

static void *
win32_output_init(G_GNUC_UNUSED const struct audio_format *audio_format,
		  G_GNUC_UNUSED const struct config_param *param,
		  G_GNUC_UNUSED GError **error)
{
	struct win32_output *wo = g_new(struct win32_output, 1);

	return wo;
}

static void
win32_output_finish(void *data)
{
	struct win32_output *wo = data;

	g_free(wo);
}

static bool
win32_output_open(void *data, struct audio_format *audio_format,
		  GError **error_r)
{
	struct win32_output *wo = data;

	wo->event = CreateEvent(NULL, false, false, NULL);
	if (wo->event == NULL) {
		g_set_error(error_r, win32_output_quark(), 0,
			    "CreateEvent() failed");
		return false;
	}

	switch (audio_format->format) {
	case SAMPLE_FORMAT_S8:
	case SAMPLE_FORMAT_S16:
		break;

	case SAMPLE_FORMAT_S24:
	case SAMPLE_FORMAT_S24_P32:
	case SAMPLE_FORMAT_S32:
	case SAMPLE_FORMAT_UNDEFINED:
		/* we havn't tested formats other than S16 */
		audio_format->format = SAMPLE_FORMAT_S16;
		break;
	}

	if (audio_format->channels > 2)
		/* same here: more than stereo was not tested */
		audio_format->channels = 2;

	WAVEFORMATEX format;
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = audio_format->channels;
	format.nSamplesPerSec = audio_format->sample_rate;
	format.nBlockAlign = audio_format_frame_size(audio_format);
	format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
	format.wBitsPerSample = audio_format_sample_size(audio_format) * 8;
	format.cbSize = 0;

	MMRESULT result = waveOutOpen(&wo->handle, WAVE_MAPPER, &format,
				      (DWORD_PTR)wo->event, 0, CALLBACK_EVENT);
	if (result != MMSYSERR_NOERROR) {
		CloseHandle(wo->event);
		g_set_error(error_r, win32_output_quark(), result,
			    "waveOutOpen() failed");
		return false;
	}

	for (unsigned i = 0; i < G_N_ELEMENTS(wo->buffers); ++i) {
		pcm_buffer_init(&wo->buffers[i].buffer);
		memset(&wo->buffers[i].hdr, 0, sizeof(wo->buffers[i].hdr));
	}

	wo->next_buffer = 0;

	return true;
}

static void
win32_output_close(void *data)
{
	struct win32_output *wo = data;

	for (unsigned i = 0; i < G_N_ELEMENTS(wo->buffers); ++i)
		pcm_buffer_deinit(&wo->buffers[i].buffer);

	waveOutClose(wo->handle);

	CloseHandle(wo->event);
}

/**
 * Copy data into a buffer, and prepare the wave header.
 */
static bool
win32_set_buffer(struct win32_output *wo, struct win32_buffer *buffer,
		 const void *data, size_t size,
		 GError **error_r)
{
	void *dest = pcm_buffer_get(&buffer->buffer, size);
	if (dest == NULL) {
		g_set_error(error_r, win32_output_quark(), 0,
			    "Out of memory");
		return false;
	}

	memcpy(dest, data, size);

	memset(&buffer->hdr, 0, sizeof(buffer->hdr));
	buffer->hdr.lpData = dest;
	buffer->hdr.dwBufferLength = size;

	MMRESULT result = waveOutPrepareHeader(wo->handle, &buffer->hdr,
					       sizeof(buffer->hdr));
	if (result != MMSYSERR_NOERROR) {
		g_set_error(error_r, win32_output_quark(), result,
			    "waveOutPrepareHeader() failed");
		return false;
	}

	return true;
}

/**
 * Wait until the buffer is finished.
 */
static bool
win32_drain_buffer(struct win32_output *wo, struct win32_buffer *buffer,
		   GError **error_r)
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
			g_set_error(error_r, win32_output_quark(), result,
				    "waveOutUnprepareHeader() failed");
			return false;
		}

		/* wait some more */
		WaitForSingleObject(wo->event, INFINITE);
	}
}

static size_t
win32_output_play(void *data, const void *chunk, size_t size, GError **error_r)
{
	struct win32_output *wo = data;

	/* get the next buffer from the ring and prepare it */
	struct win32_buffer *buffer = &wo->buffers[wo->next_buffer];
	if (!win32_drain_buffer(wo, buffer, error_r) ||
	    !win32_set_buffer(wo, buffer, chunk, size, error_r))
		return 0;

	/* enqueue the buffer */
	MMRESULT result = waveOutWrite(wo->handle, &buffer->hdr,
				       sizeof(buffer->hdr));
	if (result != MMSYSERR_NOERROR) {
		waveOutUnprepareHeader(wo->handle, &buffer->hdr,
				       sizeof(buffer->hdr));
		g_set_error(error_r, win32_output_quark(), result,
			    "waveOutWrite() failed");
		return 0;
	}

	/* mark our buffer as "used" */
	wo->next_buffer = (wo->next_buffer + 1) %
		G_N_ELEMENTS(wo->buffers);

	return size;
}

static bool
win32_drain_all_buffers(struct win32_output *wo, GError **error_r)
{
	for (unsigned i = wo->next_buffer; i < G_N_ELEMENTS(wo->buffers); ++i)
		if (!win32_drain_buffer(wo, &wo->buffers[i], error_r))
			return false;

	for (unsigned i = 0; i < wo->next_buffer; ++i)
		if (!win32_drain_buffer(wo, &wo->buffers[i], error_r))
			return false;

	return true;
}

static void
win32_stop(struct win32_output *wo)
{
	waveOutReset(wo->handle);

	for (unsigned i = 0; i < G_N_ELEMENTS(wo->buffers); ++i) {
		struct win32_buffer *buffer = &wo->buffers[i];
		waveOutUnprepareHeader(wo->handle, &buffer->hdr,
				       sizeof(buffer->hdr));
	}
}

static void
win32_output_drain(void *data)
{
	struct win32_output *wo = data;

	if (!win32_drain_all_buffers(wo, NULL))
		win32_stop(wo);
}

static void
win32_output_cancel(void *data)
{
	struct win32_output *wo = data;

	win32_stop(wo);
}

const struct audio_output_plugin win32_output_plugin = {
	.name = "win32",
	.test_default_device = win32_output_test_default_device,
	.init = win32_output_init,
	.finish = win32_output_finish,
	.open = win32_output_open,
	.close = win32_output_close,
	.play = win32_output_play,
	.drain = win32_output_drain,
	.cancel = win32_output_cancel,
};
