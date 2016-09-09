/*
 * Copyright 2003-2016 The Music Player Daemon Project
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
#include "../InputPlugin.hxx"
#include "../AsyncInputStream.hxx"
#include "util/Domain.hxx"
#include "util/Error.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringCompare.hxx"
#include "util/ReusableArray.hxx"
#include "util/ScopeExit.hxx"

#include "Log.hxx"
#include "event/MultiSocketMonitor.hxx"
#include "event/DeferredMonitor.hxx"
#include "IOThread.hxx"

#include <alsa/asoundlib.h>

#include <assert.h>
#include <string.h>

static constexpr Domain alsa_input_domain("alsa");

static constexpr const char *default_device = "hw:0,0";

// the following defaults are because the PcmDecoderPlugin forces CD format
static constexpr snd_pcm_format_t default_format = SND_PCM_FORMAT_S16;
static constexpr int default_channels = 2; // stereo
static constexpr unsigned int default_rate = 44100; // cd quality

static constexpr size_t ALSA_MAX_BUFFERED = default_rate * default_channels * 2;
static constexpr size_t ALSA_RESUME_AT = ALSA_MAX_BUFFERED / 2;

/**
 * This value should be the same as the read buffer size defined in
 * PcmDecoderPlugin.cxx:pcm_stream_decode().
 * We use it to calculate how many audio frames to buffer in the alsa driver
 * before reading from the device. snd_pcm_readi() blocks until that many
 * frames are ready.
 */
static constexpr size_t read_buffer_size = 4096;

class AlsaInputStream final
	: public AsyncInputStream,
	  MultiSocketMonitor, DeferredMonitor {
	snd_pcm_t *capture_handle;
	size_t frame_size;

	ReusableArray<pollfd> pfd_buffer;

public:
	AlsaInputStream(EventLoop &loop,
			const char *_uri, Mutex &_mutex, Cond &_cond,
			snd_pcm_t *_handle, int _frame_size)
		:AsyncInputStream(_uri, _mutex, _cond,
				  ALSA_MAX_BUFFERED, ALSA_RESUME_AT),
		 MultiSocketMonitor(loop),
		 DeferredMonitor(loop),
		 capture_handle(_handle),
		 frame_size(_frame_size)
	{
		assert(_uri != nullptr);
		assert(_handle != nullptr);

		/* this mime type forces use of the PcmDecoderPlugin.
		   Needs to be generalised when/if that decoder is
		   updated to support other audio formats */
		SetMimeType("audio/x-mpd-cdda-pcm");
		InputStream::SetReady();

		snd_pcm_start(capture_handle);

		DeferredMonitor::Schedule();
	}

	~AlsaInputStream() {
		snd_pcm_close(capture_handle);
	}

	static InputStream *Create(const char *uri, Mutex &mutex, Cond &cond);

protected:
	/* virtual methods from AsyncInputStream */
	virtual void DoResume() override {
		snd_pcm_resume(capture_handle);

		InvalidateSockets();
	}

	virtual void DoSeek(gcc_unused offset_type new_offset) override {
		/* unreachable because seekable==false */
		SeekDone();
	}

private:
	static snd_pcm_t *OpenDevice(const char *device, int rate,
				     snd_pcm_format_t format, int channels);

	void Pause() {
		AsyncInputStream::Pause();
		InvalidateSockets();
	}

	int Recover(int err);

	void SafeInvalidateSockets() {
		DeferredMonitor::Schedule();
	}

	virtual void RunDeferred() override {
		InvalidateSockets();
	}

	virtual int PrepareSockets() override;
	virtual void DispatchSockets() override;
};

inline InputStream *
AlsaInputStream::Create(const char *uri, Mutex &mutex, Cond &cond)
{
	const char *device = StringAfterPrefix(uri, "alsa://");
	if (device == nullptr)
		return nullptr;

	if (*device == 0)
		device = default_device;

	/* placeholders - eventually user-requested audio format will
	   be passed via the URI. For now we just force the
	   defaults */
	int rate = default_rate;
	snd_pcm_format_t format = default_format;
	int channels = default_channels;

	snd_pcm_t *handle = OpenDevice(device, rate, format, channels);

	int frame_size = snd_pcm_format_width(format) / 8 * channels;
	return new AlsaInputStream(io_thread_get(),
				   uri, mutex, cond,
				   handle, frame_size);
}

int
AlsaInputStream::PrepareSockets()
{
	if (IsPaused()) {
		ClearSocketList();
		return -1;
	}

	int count = snd_pcm_poll_descriptors_count(capture_handle);
	if (count < 0) {
		ClearSocketList();
		return -1;
	}

	struct pollfd *pfds = pfd_buffer.Get(count);

	count = snd_pcm_poll_descriptors(capture_handle, pfds, count);
	if (count < 0)
		count = 0;

	ReplaceSocketList(pfds, count);
	return -1;
}

void
AlsaInputStream::DispatchSockets()
{
	const ScopeLock protect(mutex);

	auto w = PrepareWriteBuffer();
	const snd_pcm_uframes_t w_frames = w.size / frame_size;
	if (w_frames == 0) {
		/* buffer is full */
		Pause();
		return;
	}

	snd_pcm_sframes_t n_frames;
	while ((n_frames = snd_pcm_readi(capture_handle,
					 w.data, w_frames)) < 0) {
		if (Recover(n_frames) < 0) {
			Error error;
			error.Format(alsa_input_domain,
				     "PCM error - stream aborted");
			PostponeError(std::move(error));
			return;
		}
	}

	size_t nbytes = n_frames * frame_size;
	CommitWriteBuffer(nbytes);
}

inline int
AlsaInputStream::Recover(int err)
{
	switch(err) {
	case -EPIPE:
		LogDebug(alsa_input_domain, "Buffer Overrun");
		// drop through
	case -ESTRPIPE:
	case -EINTR:
		err = snd_pcm_recover(capture_handle, err, 1);
		break;
	default:
		// something broken somewhere, give up
		err = -1;
	}
	return err;
}

static void
ConfigureCapture(snd_pcm_t *capture_handle,
		 int rate, snd_pcm_format_t format, int channels)
{
	int err;

	snd_pcm_hw_params_t *hw_params;
	if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0)
		throw FormatRuntimeError("Cannot allocate hardware parameter structure (%s)",
					 snd_strerror(err));

	AtScopeExit(hw_params) {
		snd_pcm_hw_params_free(hw_params);
	};

	if ((err = snd_pcm_hw_params_any(capture_handle, hw_params)) < 0)
		throw FormatRuntimeError("Cannot initialize hardware parameter structure (%s)",
					 snd_strerror(err));

	if ((err = snd_pcm_hw_params_set_access(capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
		throw FormatRuntimeError("Cannot set access type (%s)",
					 snd_strerror(err));

	if ((err = snd_pcm_hw_params_set_format(capture_handle, hw_params, format)) < 0)
		throw FormatRuntimeError("Cannot set sample format (%s)",
					 snd_strerror(err));

	if ((err = snd_pcm_hw_params_set_channels(capture_handle, hw_params, channels)) < 0)
		throw FormatRuntimeError("Cannot set channels (%s)",
					 snd_strerror(err));

	if ((err = snd_pcm_hw_params_set_rate(capture_handle, hw_params, rate, 0)) < 0)
		throw FormatRuntimeError("Cannot set sample rate (%s)",
					 snd_strerror(err));

	/* period needs to be big enough so that poll() doesn't fire too often,
	 * but small enough that buffer overruns don't occur if Read() is not
	 * invoked often enough.
	 * the calculation here is empirical; however all measurements were
	 * done using 44100:16:2. When we extend this plugin to support
	 * other audio formats then this may need to be revisited */
	snd_pcm_uframes_t period = read_buffer_size * 2;
	int direction = -1;
	if ((err = snd_pcm_hw_params_set_period_size_near(capture_handle, hw_params,
							  &period, &direction)) < 0)
		throw FormatRuntimeError("Cannot set period size (%s)",
					 snd_strerror(err));

	if ((err = snd_pcm_hw_params(capture_handle, hw_params)) < 0)
		throw FormatRuntimeError("Cannot set parameters (%s)",
					 snd_strerror(err));

	snd_pcm_sw_params_t *sw_params;

	snd_pcm_sw_params_malloc(&sw_params);
	snd_pcm_sw_params_current(capture_handle, sw_params);

	AtScopeExit(sw_params) {
		snd_pcm_sw_params_free(sw_params);
	};

	if ((err = snd_pcm_sw_params_set_start_threshold(capture_handle, sw_params,
							 period)) < 0)
		throw FormatRuntimeError("unable to set start threshold (%s)",
					 snd_strerror(err));

	if ((err = snd_pcm_sw_params(capture_handle, sw_params)) < 0)
		throw FormatRuntimeError("unable to install sw params (%s)",
					 snd_strerror(err));
}

inline snd_pcm_t *
AlsaInputStream::OpenDevice(const char *device,
			    int rate, snd_pcm_format_t format, int channels)
{
	snd_pcm_t *capture_handle;
	int err;
	if ((err = snd_pcm_open(&capture_handle, device,
				SND_PCM_STREAM_CAPTURE, 0)) < 0)
		throw FormatRuntimeError("Failed to open device: %s (%s)",
					 device, snd_strerror(err));

	try {
		ConfigureCapture(capture_handle, rate, format, channels);
	} catch (...) {
		snd_pcm_close(capture_handle);
		throw;
	}

	snd_pcm_prepare(capture_handle);

	return capture_handle;
}

/*#########################  Plugin Functions  ##############################*/

static InputStream *
alsa_input_open(const char *uri, Mutex &mutex, Cond &cond)
{
	return AlsaInputStream::Create(uri, mutex, cond);
}

const struct InputPlugin input_plugin_alsa = {
	"alsa",
	nullptr,
	nullptr,
	alsa_input_open,
};
