/*
 * Copyright 2003-2018 The Music Player Daemon Project
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

#include "AlsaInputPlugin.hxx"
#include "lib/alsa/NonBlock.hxx"
#include "../InputPlugin.hxx"
#include "../AsyncInputStream.hxx"
#include "event/Call.hxx"
#include "thread/Cond.hxx"
#include "util/Domain.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringCompare.hxx"
#include "util/ASCII.hxx"
#include "Log.hxx"
#include "event/MultiSocketMonitor.hxx"
#include "event/DeferEvent.hxx"

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

class AlsaInputStream final
	: public AsyncInputStream,
	  MultiSocketMonitor {

	/**
	 * The configured name of the ALSA device.
	 */
	const std::string device;

	snd_pcm_t *const capture_handle;
	const size_t frame_size;

	AlsaNonBlockPcm non_block;

	DeferEvent defer_invalidate_sockets;

public:
	AlsaInputStream(EventLoop &_loop,
			const char *_uri, Mutex &_mutex,
			const char *_device,
			snd_pcm_t *_handle, int _frame_size)
		:AsyncInputStream(_loop, _uri, _mutex,
				  ALSA_MAX_BUFFERED, ALSA_RESUME_AT),
		 MultiSocketMonitor(_loop),
		 device(_device),
		 capture_handle(_handle),
		 frame_size(_frame_size),
		 defer_invalidate_sockets(_loop,
					  BIND_THIS_METHOD(InvalidateSockets))
	{
		assert(_uri != nullptr);
		assert(_handle != nullptr);

		/* this mime type forces use of the PcmDecoderPlugin.
		   Needs to be generalised when/if that decoder is
		   updated to support other audio formats */
		SetMimeType("audio/x-mpd-cdda-pcm");
		InputStream::SetReady();

		snd_pcm_start(capture_handle);

		defer_invalidate_sockets.Schedule();
	}

	~AlsaInputStream() {
		BlockingCall(MultiSocketMonitor::GetEventLoop(), [this](){
				MultiSocketMonitor::Reset();
				defer_invalidate_sockets.Cancel();
			});

		snd_pcm_close(capture_handle);
	}

	static InputStreamPtr Create(EventLoop &event_loop, const char *uri,
				     Mutex &mutex);

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
		defer_invalidate_sockets.Schedule();
	}

	/* virtual methods from class MultiSocketMonitor */
	std::chrono::steady_clock::duration PrepareSockets() noexcept override;
	void DispatchSockets() noexcept override;
};

inline InputStreamPtr
AlsaInputStream::Create(EventLoop &event_loop, const char *uri,
			Mutex &mutex)
{
	const char *device = StringAfterPrefixCaseASCII(uri, "alsa://");
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
	return std::make_unique<AlsaInputStream>(event_loop,
						 uri, mutex,
						 device, handle, frame_size);
}

std::chrono::steady_clock::duration
AlsaInputStream::PrepareSockets() noexcept
{
	if (IsPaused()) {
		ClearSocketList();
		return std::chrono::steady_clock::duration(-1);
	}

	return non_block.PrepareSockets(*this, capture_handle);
}

void
AlsaInputStream::DispatchSockets() noexcept
{
	non_block.DispatchSockets(*this, capture_handle);

	const std::lock_guard<Mutex> protect(mutex);

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
		if (n_frames == -EAGAIN)
			return;

		if (Recover(n_frames) < 0) {
			postponed_exception = std::make_exception_ptr(std::runtime_error("PCM error - stream aborted"));
			InvokeOnAvailable();
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
		FormatDebug(alsa_input_domain,
			    "Overrun on ALSA capture device \"%s\"",
			    device.c_str());
		break;

	case -ESTRPIPE:
		FormatDebug(alsa_input_domain,
			    "ALSA capture device \"%s\" was suspended",
			    device.c_str());
		break;
	}

	switch (snd_pcm_state(capture_handle)) {
	case SND_PCM_STATE_PAUSED:
		err = snd_pcm_pause(capture_handle, /* disable */ 0);
		break;

	case SND_PCM_STATE_SUSPENDED:
		err = snd_pcm_resume(capture_handle);
		if (err == -EAGAIN)
			return 0;
		/* fall-through to snd_pcm_prepare: */
#if GCC_CHECK_VERSION(7,0)
		[[fallthrough]];
#endif
	case SND_PCM_STATE_OPEN:
	case SND_PCM_STATE_SETUP:
	case SND_PCM_STATE_XRUN:
		err = snd_pcm_prepare(capture_handle);
		if (err == 0)
			err = snd_pcm_start(capture_handle);
		break;

	case SND_PCM_STATE_DISCONNECTED:
		break;

	case SND_PCM_STATE_PREPARED:
	case SND_PCM_STATE_RUNNING:
	case SND_PCM_STATE_DRAINING:
		/* this is no error, so just keep running */
		err = 0;
		break;

	default:
		/* this default case is just here to work around
		   -Wswitch due to SND_PCM_STATE_PRIVATE1 (libasound
		   1.1.6) */
		break;
	}


	return err;
}

static void
ConfigureCapture(snd_pcm_t *capture_handle,
		 int rate, snd_pcm_format_t format, int channels)
{
	int err;

	snd_pcm_hw_params_t *hw_params;
	snd_pcm_hw_params_alloca(&hw_params);

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

	snd_pcm_uframes_t buffer_size_min, buffer_size_max;
	snd_pcm_hw_params_get_buffer_size_min(hw_params, &buffer_size_min);
	snd_pcm_hw_params_get_buffer_size_max(hw_params, &buffer_size_max);
	unsigned buffer_time_min, buffer_time_max;
	snd_pcm_hw_params_get_buffer_time_min(hw_params, &buffer_time_min, 0);
	snd_pcm_hw_params_get_buffer_time_max(hw_params, &buffer_time_max, 0);
	FormatDebug(alsa_input_domain, "buffer: size=%u..%u time=%u..%u",
		    (unsigned)buffer_size_min, (unsigned)buffer_size_max,
		    buffer_time_min, buffer_time_max);

	snd_pcm_uframes_t period_size_min, period_size_max;
	snd_pcm_hw_params_get_period_size_min(hw_params, &period_size_min, 0);
	snd_pcm_hw_params_get_period_size_max(hw_params, &period_size_max, 0);
	unsigned period_time_min, period_time_max;
	snd_pcm_hw_params_get_period_time_min(hw_params, &period_time_min, 0);
	snd_pcm_hw_params_get_period_time_max(hw_params, &period_time_max, 0);
	FormatDebug(alsa_input_domain, "period: size=%u..%u time=%u..%u",
		    (unsigned)period_size_min, (unsigned)period_size_max,
		    period_time_min, period_time_max);

	/* choose the maximum possible buffer_size ... */
	snd_pcm_hw_params_set_buffer_size(capture_handle, hw_params,
					  buffer_size_max);

	/* ... and calculate the period_size to have four periods in
	   one buffer; this way, we get woken up often enough to avoid
	   buffer overruns, but not too often */
	snd_pcm_uframes_t buffer_size;
	if (snd_pcm_hw_params_get_buffer_size(hw_params, &buffer_size) == 0) {
		snd_pcm_uframes_t period_size = buffer_size / 4;
		int direction = -1;
		if ((err = snd_pcm_hw_params_set_period_size_near(capture_handle, hw_params,
								  &period_size, &direction)) < 0)
			throw FormatRuntimeError("Cannot set period size (%s)",
						 snd_strerror(err));
	}

	if ((err = snd_pcm_hw_params(capture_handle, hw_params)) < 0)
		throw FormatRuntimeError("Cannot set parameters (%s)",
					 snd_strerror(err));

	snd_pcm_uframes_t alsa_buffer_size;
	err = snd_pcm_hw_params_get_buffer_size(hw_params, &alsa_buffer_size);
	if (err < 0)
		throw FormatRuntimeError("snd_pcm_hw_params_get_buffer_size() failed: %s",
					 snd_strerror(-err));

	snd_pcm_uframes_t alsa_period_size;
	err = snd_pcm_hw_params_get_period_size(hw_params, &alsa_period_size,
						nullptr);
	if (err < 0)
		throw FormatRuntimeError("snd_pcm_hw_params_get_period_size() failed: %s",
					 snd_strerror(-err));

	FormatDebug(alsa_input_domain, "buffer_size=%u period_size=%u",
		    (unsigned)alsa_buffer_size, (unsigned)alsa_period_size);

	snd_pcm_sw_params_t *sw_params;
	snd_pcm_sw_params_alloca(&sw_params);

	snd_pcm_sw_params_current(capture_handle, sw_params);

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
				SND_PCM_STREAM_CAPTURE,
				SND_PCM_NONBLOCK)) < 0)
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

static EventLoop *alsa_input_event_loop;

static void
alsa_input_init(EventLoop &event_loop, const ConfigBlock &)
{
	alsa_input_event_loop = &event_loop;
}

static InputStreamPtr
alsa_input_open(const char *uri, Mutex &mutex)
{
	return AlsaInputStream::Create(*alsa_input_event_loop, uri,
				       mutex);
}

static constexpr const char *alsa_prefixes[] = {
	"alsa://",
	nullptr
};

const struct InputPlugin input_plugin_alsa = {
	"alsa",
	alsa_prefixes,
	alsa_input_init,
	nullptr,
	alsa_input_open,
};
