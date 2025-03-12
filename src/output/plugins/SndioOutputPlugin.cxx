// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "SndioOutputPlugin.hxx"
#include "mixer/Listener.hxx"
#include "mixer/plugins/SndioMixerPlugin.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <sndio.h>

#include <stdexcept>

#ifndef SIO_DEVANY
/* this macro is missing in libroar-dev 1.0~beta2-3 (Debian Wheezy) */
#define SIO_DEVANY "default"
#endif

static constexpr unsigned MPD_SNDIO_BUFFER_TIME_MS = 250;

static constexpr Domain sndio_output_domain("sndio_output");

SndioOutput::SndioOutput(const ConfigBlock &block)
	:AudioOutput(0),
	 device(block.GetBlockValue("device", SIO_DEVANY)),
	 buffer_time(block.GetBlockValue("buffer_time",
					 MPD_SNDIO_BUFFER_TIME_MS)),
	 raw_volume(SIO_MAXVOL)
{
}

static void
VolumeCallback(void *arg, unsigned int volume) {
	((SndioOutput *)arg)->VolumeChanged(volume);
}

AudioOutput *
SndioOutput::Create(EventLoop &, const ConfigBlock &block) {
	return new SndioOutput(block);
}

static bool
sndio_test_default_device()
{
	auto *hdl = sio_open(SIO_DEVANY, SIO_PLAY, 0);
	if (!hdl) {
		LogError(sndio_output_domain,
			 "Error opening default sndio device");
		return false;
	}

	sio_close(hdl);
	return true;
}

void
SndioOutput::Open(AudioFormat &audio_format)
{
	struct sio_par par;
	unsigned bits, rate, chans;

	hdl = sio_open(device, SIO_PLAY, 0);
	if (!hdl)
		throw std::runtime_error("Failed to open default sndio device");

	switch (audio_format.format) {
	case SampleFormat::S16:
		bits = 16;
		break;
	case SampleFormat::S24_P32:
		bits = 24;
		break;
	case SampleFormat::S32:
		bits = 32;
		break;
	default:
		audio_format.format = SampleFormat::S16;
		bits = 16;
		break;
	}

	rate = audio_format.sample_rate;
	chans = audio_format.channels;

	sio_initpar(&par);
	par.bits = bits;
	par.rate = rate;
	par.pchan = chans;
	par.sig = 1;
	par.le = SIO_LE_NATIVE;
	par.appbufsz = rate * buffer_time / 1000;

	if (!sio_setpar(hdl, &par) ||
	    !sio_getpar(hdl, &par)) {
		sio_close(hdl);
		throw std::runtime_error("Failed to set/get audio params");
	}

	if (par.bits != bits ||
	    par.rate < rate * 995 / 1000 ||
	    par.rate > rate * 1005 / 1000 ||
	    par.pchan != chans ||
	    par.sig != 1 ||
	    par.le != SIO_LE_NATIVE) {
		sio_close(hdl);
		throw std::runtime_error("Requested audio params cannot be satisfied");
	}

	// Set volume after opening fresh audio stream which does
	// know nothing about previous audio streams.
	sio_setvol(hdl, raw_volume);
	// sio_onvol returns 0 if no volume knob is available.
	// This is the case on raw audio devices rather than
	// the sndiod audio server.
	if (sio_onvol(hdl, VolumeCallback, this) == 0)
		raw_volume = -1;

	if (!sio_start(hdl)) {
		sio_close(hdl);
		throw std::runtime_error("Failed to start audio device");
	}
}

void
SndioOutput::Close()  noexcept
{
	sio_close(hdl);
}

size_t
SndioOutput::Play(std::span<const std::byte> src)
{
	const std::size_t n = sio_write(hdl, src.data(), src.size());
	if (n == 0 && sio_eof(hdl) != 0)
		throw std::runtime_error("sndio write failed");
	return n;
}

void
SndioOutput::SetVolume(unsigned int volume)
{
	sio_setvol(hdl, (volume * SIO_MAXVOL + 50) / 100);
}

static inline unsigned int
RawToPercent(int raw_volume) {
	return raw_volume < 0 ? 100 :
	(raw_volume * 100 + SIO_MAXVOL / 2) / SIO_MAXVOL;
}

void
SndioOutput::VolumeChanged(int _raw_volume) {
	if (raw_volume >= 0 && listener != nullptr && mixer != nullptr) {
		raw_volume = _raw_volume;
		listener->OnMixerVolumeChanged(*mixer,
		    RawToPercent(raw_volume));
	}
}

unsigned int
SndioOutput::GetVolume() {
	return RawToPercent(raw_volume);
}

void
SndioOutput::RegisterMixerListener(Mixer *_mixer, MixerListener *_listener) {
	mixer = _mixer;
	listener = _listener;
}

constexpr struct AudioOutputPlugin sndio_output_plugin = {
	"sndio",
	sndio_test_default_device,
	SndioOutput::Create,
	&sndio_mixer_plugin,
};
