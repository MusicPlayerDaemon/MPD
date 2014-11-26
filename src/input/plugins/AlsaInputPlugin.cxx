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

/*
 * ALSA code based on an example by Paul Davis released under GPL here:
 * http://equalarea.com/paul/alsa-audio.html
 * and one by Matthias Nagorni, also GPL, here:
 * http://alsamodular.sourceforge.net/alsa_programming_howto.html
 */

#include "config.h"
#include "AlsaInputPlugin.hxx"
#include "../InputPlugin.hxx"
#include "../InputStream.hxx"
#include "util/Domain.hxx"
#include "util/Error.hxx"
#include "util/StringUtil.hxx"
#include "util/ReusableArray.hxx"

#include "Log.hxx"
#include "event/MultiSocketMonitor.hxx"
#include "event/DeferredMonitor.hxx"
#include "event/Call.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "IOThread.hxx"

#include <alsa/asoundlib.h>

#include <atomic>

#include <assert.h>
#include <string.h>

static constexpr Domain alsa_input_domain("alsa");

static constexpr const char *default_device = "hw:0,0";

// the following defaults are because the PcmDecoderPlugin forces CD format
static constexpr snd_pcm_format_t default_format = SND_PCM_FORMAT_S16;
static constexpr int default_channels = 2; // stereo
static constexpr unsigned int default_rate = 44100; // cd quality

/**
 * This value should be the same as the read buffer size defined in
 * PcmDecoderPlugin.cxx:pcm_stream_decode().
 * We use it to calculate how many audio frames to buffer in the alsa driver
 * before reading from the device. snd_pcm_readi() blocks until that many
 * frames are ready.
 */
static constexpr size_t read_buffer_size = 4096;

class AlsaInputStream final
	: public InputStream,
	  MultiSocketMonitor, DeferredMonitor {
	snd_pcm_t *capture_handle;
	size_t frame_size;
	int frames_to_read;
	bool eof;

	/**
	 * Is somebody waiting for data?  This is set by method
	 * Available().
	 */
	std::atomic_bool waiting;

	ReusableArray<pollfd> pfd_buffer;

public:
	AlsaInputStream(EventLoop &loop,
			const char *_uri, Mutex &_mutex, Cond &_cond,
			snd_pcm_t *_handle, int _frame_size)
		:InputStream(_uri, _mutex, _cond),
		 MultiSocketMonitor(loop),
		 DeferredMonitor(loop),
		 capture_handle(_handle),
		 frame_size(_frame_size),
		 eof(false)
	{
		assert(_uri != nullptr);
		assert(_handle != nullptr);

		/* this mime type forces use of the PcmDecoderPlugin.
		   Needs to be generalised when/if that decoder is
		   updated to support other audio formats */
		SetMimeType("audio/x-mpd-cdda-pcm");
		InputStream::SetReady();

		frames_to_read = read_buffer_size / frame_size;

		snd_pcm_start(capture_handle);

		DeferredMonitor::Schedule();
	}

	~AlsaInputStream() {
		snd_pcm_close(capture_handle);
	}

	using DeferredMonitor::GetEventLoop;

	static InputStream *Create(const char *uri, Mutex &mutex, Cond &cond,
				   Error &error);

	/* virtual methods from InputStream */

	bool IsEOF() override {
		return eof;
	}

	bool IsAvailable() override {
		if (snd_pcm_avail(capture_handle) > frames_to_read)
			return true;

		if (!waiting.exchange(true))
			SafeInvalidateSockets();

		return false;
	}

	size_t Read(void *ptr, size_t size, Error &error) override;

private:
	static snd_pcm_t *OpenDevice(const char *device, int rate,
				     snd_pcm_format_t format, int channels,
				     Error &error);

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
AlsaInputStream::Create(const char *uri, Mutex &mutex, Cond &cond,
			Error &error)
{
	const char *const scheme = "alsa://";
	if (!StringStartsWith(uri, scheme))
		return nullptr;

	const char *device = uri + strlen(scheme);
	if (strlen(device) == 0)
		device = default_device;

	/* placeholders - eventually user-requested audio format will
	   be passed via the URI. For now we just force the
	   defaults */
	int rate = default_rate;
	snd_pcm_format_t format = default_format;
	int channels = default_channels;

	snd_pcm_t *handle = OpenDevice(device, rate, format, channels,
				       error);
	if (handle == nullptr)
		return nullptr;

	int frame_size = snd_pcm_format_width(format) / 8 * channels;
	return new AlsaInputStream(io_thread_get(),
				   uri, mutex, cond,
				   handle, frame_size);
}

size_t
AlsaInputStream::Read(void *ptr, size_t read_size, Error &error)
{
	assert(ptr != nullptr);

	int num_frames = read_size / frame_size;
	int ret;
	while ((ret = snd_pcm_readi(capture_handle, ptr, num_frames)) < 0) {
		if (Recover(ret) < 0) {
			eof = true;
			error.Format(alsa_input_domain,
				     "PCM error - stream aborted");
			return 0;
		}
	}

	size_t nbytes = ret * frame_size;
	offset += nbytes;
	return nbytes;
}

int
AlsaInputStream::PrepareSockets()
{
	if (!waiting) {
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
	waiting = false;

	const ScopeLock protect(mutex);
	/* wake up the thread that is waiting for more data */
	cond.broadcast();
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

inline snd_pcm_t *
AlsaInputStream::OpenDevice(const char *device,
			    int rate, snd_pcm_format_t format, int channels,
			    Error &error)
{
	snd_pcm_t *capture_handle;
	int err;
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
		snd_pcm_hw_params_free(hw_params);
		snd_pcm_close(capture_handle);
		return nullptr;
	}

	if ((err = snd_pcm_hw_params_set_format(capture_handle, hw_params, format)) < 0) {
		snd_pcm_hw_params_free(hw_params);
		snd_pcm_close(capture_handle);
		error.Format(alsa_input_domain, "Cannot set sample format (%s)", snd_strerror (err));
		return nullptr;
	}

	if ((err = snd_pcm_hw_params_set_channels(capture_handle, hw_params, channels)) < 0) {
		snd_pcm_hw_params_free(hw_params);
		snd_pcm_close(capture_handle);
		error.Format(alsa_input_domain, "Cannot set channels (%s)", snd_strerror (err));
		return nullptr;
	}

	if ((err = snd_pcm_hw_params_set_rate(capture_handle, hw_params, rate, 0)) < 0) {
		snd_pcm_hw_params_free(hw_params);
		snd_pcm_close(capture_handle);
		error.Format(alsa_input_domain, "Cannot set sample rate (%s)", snd_strerror (err));
		return nullptr;
	}

	/* period needs to be big enough so that poll() doesn't fire too often,
	 * but small enough that buffer overruns don't occur if Read() is not
	 * invoked often enough.
	 * the calculation here is empirical; however all measurements were
	 * done using 44100:16:2. When we extend this plugin to support
	 * other audio formats then this may need to be revisited */
	snd_pcm_uframes_t period = read_buffer_size * 2;
	int direction = -1;
	if ((err = snd_pcm_hw_params_set_period_size_near(capture_handle, hw_params,
							  &period, &direction)) < 0) {
		error.Format(alsa_input_domain, "Cannot set period size (%s)",
			     snd_strerror(err));
		snd_pcm_hw_params_free(hw_params);
		snd_pcm_close(capture_handle);
		return nullptr;
	}

	if ((err = snd_pcm_hw_params(capture_handle, hw_params)) < 0) {
		error.Format(alsa_input_domain, "Cannot set parameters (%s)",
			     snd_strerror(err));
		snd_pcm_hw_params_free(hw_params);
		snd_pcm_close(capture_handle);
		return nullptr;
	}

	snd_pcm_hw_params_free (hw_params);

	snd_pcm_sw_params_t *sw_params;

	snd_pcm_sw_params_malloc(&sw_params);
	snd_pcm_sw_params_current(capture_handle, sw_params);

	if ((err = snd_pcm_sw_params_set_start_threshold(capture_handle, sw_params,
							 period)) < 0)  {
		error.Format(alsa_input_domain,
			     "unable to set start threshold (%s)", snd_strerror(err));
		snd_pcm_sw_params_free(sw_params);
		snd_pcm_close(capture_handle);
		return nullptr;
	}

	if ((err = snd_pcm_sw_params(capture_handle, sw_params)) < 0) {
		error.Format(alsa_input_domain,
			     "unable to install sw params (%s)", snd_strerror(err));
		snd_pcm_sw_params_free(sw_params);
		snd_pcm_close(capture_handle);
		return nullptr;
	}

	snd_pcm_sw_params_free(sw_params);

	snd_pcm_prepare(capture_handle);

	return capture_handle;
}

/*#########################  Plugin Functions  ##############################*/

static InputStream *
alsa_input_open(const char *uri, Mutex &mutex, Cond &cond, Error &error)
{
	return AlsaInputStream::Create(uri, mutex, cond, error);
}

const struct InputPlugin input_plugin_alsa = {
	"alsa",
	nullptr,
	nullptr,
	alsa_input_open,
};
