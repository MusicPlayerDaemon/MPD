/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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

/*
 * ALSA code based on an example by Paul Davis released under GPL here:
 * http://equalarea.com/paul/alsa-audio.html
 * and one by Matthias Nagorni, also GPL, here:
 * http://alsamodular.sourceforge.net/alsa_programming_howto.html
 */

#include "config.h"
#include "AlsaInputPlugin.hxx"
#include "InputPlugin.hxx"
#include "InputStream.hxx"
#include "util/Domain.hxx"
#include "util/Error.hxx"
#include "util/StringUtil.hxx"
#include "Log.hxx"

#include <alsa/asoundlib.h>

static constexpr Domain alsa_input_domain("alsa");

static constexpr const char *default_device = "hw:0,0";

// this value chosen to balance between limiting latency and avoiding stutter
static constexpr int max_frames_to_buffer = 64;

// the following defaults are because the PcmDecoderPlugin forces CD format
static constexpr snd_pcm_format_t default_format = SND_PCM_FORMAT_S16;
static constexpr int default_channels = 2; // stereo
static constexpr unsigned int default_rate = 44100; // cd quality

struct AlsaInputStream {
	InputStream base;
	snd_pcm_t *capture_handle;
	size_t frame_size;
	size_t max_bytes_to_read;

	AlsaInputStream(const char *uri, Mutex &mutex, Cond &cond,
			snd_pcm_t *handle)
		:base(input_plugin_alsa, uri, mutex, cond),
		 capture_handle(handle) {
		frame_size = snd_pcm_format_width(default_format) / 8 * default_channels;
		max_bytes_to_read = max_frames_to_buffer * frame_size;
		base.mime = strdup("audio/x-mpd-cdda-pcm");
		base.seekable = false;
		base.size = -1;
		base.ready = true;
	}

	~AlsaInputStream() {
		snd_pcm_close(capture_handle);
	}
};

static InputStream *
alsa_input_open(const char *uri, Mutex &mutex, Cond &cond, Error &error)
{
	int err;

	// check uri is appropriate for alsa input
	if (!StringStartsWith(uri, "alsa://"))
		return nullptr;

	const char *device = uri + 7;
	if (device[0] == '\0')
		device = default_device;

	snd_pcm_t *capture_handle;
	if ((err = snd_pcm_open(&capture_handle, device,
				SND_PCM_STREAM_CAPTURE, 0)) < 0) {
		error.Format(alsa_input_domain, "Failed to open device: %s (%s)", device, snd_strerror(err));
		return nullptr;
	}

	snd_pcm_hw_params_t *hw_params;
	if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
		error.Format(alsa_input_domain, "Cannot allocate hardware parameter structure (%s)", snd_strerror(err));
		snd_pcm_close(capture_handle);
		return nullptr;
	}

	if ((err = snd_pcm_hw_params_any(capture_handle, hw_params)) < 0) {
		error.Format(alsa_input_domain, "Cannot initialize hardware parameter structure (%s)", snd_strerror(err));
		snd_pcm_hw_params_free(hw_params);
		snd_pcm_close(capture_handle);
		return nullptr;
	}

	if ((err = snd_pcm_hw_params_set_access(capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		error.Format(alsa_input_domain, "Cannot set access type (%s)", snd_strerror (err));
		snd_pcm_hw_params_free (hw_params);
		snd_pcm_close(capture_handle);
		return nullptr;
	}

	if ((err = snd_pcm_hw_params_set_format(capture_handle, hw_params, default_format)) < 0) {
		snd_pcm_hw_params_free(hw_params);
		snd_pcm_close(capture_handle);
		error.Format(alsa_input_domain, "Cannot set sample format (%s)", snd_strerror (err));
		return nullptr;
	}

	if ((err = snd_pcm_hw_params_set_channels(capture_handle, hw_params, default_channels)) < 0) {
		snd_pcm_hw_params_free(hw_params);
		snd_pcm_close(capture_handle);
		error.Format(alsa_input_domain, "Cannot set channels (%s)", snd_strerror (err));
		return nullptr;
	}

	if ((err = snd_pcm_hw_params_set_rate(capture_handle, hw_params, default_rate, 0)) < 0) {
		snd_pcm_hw_params_free(hw_params);
		snd_pcm_close(capture_handle);
		error.Format(alsa_input_domain, "Cannot set sample rate (%s)", snd_strerror (err));
		return nullptr;
	}

	if ((err = snd_pcm_hw_params(capture_handle, hw_params)) < 0) {
		error.Format(alsa_input_domain, "Cannot set parameters (%s)", snd_strerror (err));
		snd_pcm_hw_params_free(hw_params);
		snd_pcm_close(capture_handle);
		return nullptr;
	}

	snd_pcm_hw_params_free (hw_params);

	// clear any data already in the PCM buffer
	if ((err = snd_pcm_drop(capture_handle)) < 0) {
		error.Format(alsa_input_domain, "Cannot clear PCM buffer (%s)", snd_strerror (err));
		snd_pcm_hw_params_free(hw_params);
		snd_pcm_close(capture_handle);
		return nullptr;
	}

	AlsaInputStream *ais = new AlsaInputStream(uri, mutex, cond, capture_handle);
	return &ais->base;
}

static void
alsa_input_close(InputStream *is)
{
	AlsaInputStream *ais = (AlsaInputStream*) is;
	delete ais;
}

static size_t
alsa_input_read(InputStream *is, void *ptr, size_t size,
		gcc_unused Error &error)
{
	AlsaInputStream *ais = (AlsaInputStream*) is;
	int num_frames = max_frames_to_buffer;
	if (size < ais->max_bytes_to_read)
		// calculate number of whole frames that will fit in size bytes
		num_frames = size / ais->frame_size;

	int ret;
	while ((ret = snd_pcm_readi(ais->capture_handle, ptr,
				    num_frames)) < 0) {
		snd_pcm_prepare(ais->capture_handle);
		LogDebug(alsa_input_domain, "Buffer Overrun");
	}

	size_t nbytes = ret == max_frames_to_buffer
		? ais->max_bytes_to_read
		: ret * ais->frame_size;
	is->offset += nbytes;
	return nbytes;
}

static bool
alsa_input_eof(gcc_unused InputStream *is)
{
	return false;
};

const struct InputPlugin input_plugin_alsa = {
	"alsa",
	nullptr,
	nullptr,
	alsa_input_open,
	alsa_input_close,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	alsa_input_read,
	alsa_input_eof,
	nullptr,
};
