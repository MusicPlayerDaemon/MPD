// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "TinyAlsaOutputPlugin.hxx"
#include "lib/tinyalsa/Format.hxx"
#include "lib/fmt/AudioFormatFormatter.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "../OutputAPI.hxx"
#include "../Error.hxx"
#include "mixer/plugins/TinyAlsaMixerPlugin.hxx"
#include "pcm/Export.hxx"
#include "system/Error.hxx"
#include "system/EventFD.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <tinyalsa/pcm.h>

#include <algorithm> // for std::min(), std::max()
#include <array>
#include <atomic>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <memory>

#include <poll.h>

static constexpr unsigned DEFAULT_BUFFER_TIME_US = 500000;

static constexpr Domain tinyalsa_output_domain("tinyalsa_output");

struct PcmParamsDeleter {
	void operator()(struct pcm_params *params) const noexcept {
		pcm_params_free(params);
	}
};

using PcmParamsPtr = std::unique_ptr<struct pcm_params, PcmParamsDeleter>;

class TinyAlsaOutput final : AudioOutput {
	/**
	 * The configured sound card and PCM device number.
	 */
	const unsigned card, device;

	/**
	 * The configured buffer time.
	 */
	const std::chrono::microseconds buffer_time;

	/**
	 * The configured period time; zero means a quarter of
	 * #buffer_time.
	 */
	const std::chrono::microseconds period_time;

	/**
	 * Signalled by Interrupt() to wake up Play() or Drain().
	 */
	EventFD interrupt_event;

	/**
	 * Set by Interrupt(); while set, Play() and Drain() do not
	 * block.  It is cleared by Open() and Cancel(), i.e. the
	 * commands which follow an Interrupt() call.
	 */
	std::atomic_bool interrupted = false;

	PcmExport pcm_export;

	/**
	 * The tinyalsa PCM handle; only valid while the output is
	 * open.
	 */
	struct pcm *pcm;

	/**
	 * A buffer filled with one period of silence.  It is used by
	 * Drain().
	 */
	std::unique_ptr<std::byte[]> silence;

	/**
	 * The size of one frame passed to Play().
	 */
	std::size_t in_frame_size;

	/**
	 * The size of one frame passed to tinyalsa.
	 */
	std::size_t out_frame_size;

	unsigned sample_rate;

	/**
	 * The size of one period in frames.  When the PCM becomes
	 * writable, it has room for at least this many frames (this
	 * is tinyalsa's "avail_min" software parameter).
	 */
	unsigned period_frames;

	/**
	 * Was data written since the PCM was opened or stopped?
	 */
	bool written;

public:
	explicit TinyAlsaOutput(const ConfigBlock &block);

	static AudioOutput *Create(EventLoop &, const ConfigBlock &block) {
		return new TinyAlsaOutput(block);
	}

private:
	/* virtual methods from class AudioOutput */
	void Open(AudioFormat &audio_format) override;
	void Close() noexcept override;
	void Interrupt() noexcept override;
	std::size_t Play(std::span<const std::byte> src) override;
	void Drain() override;
	void Cancel() noexcept override;

	/**
	 * Wait until the PCM is writable (or in an error state which
	 * the next pcm_writei() call handles).
	 *
	 * Throws #AudioOutputInterrupted after Interrupt() has been
	 * called, and std::system_error on error.
	 */
	void WaitWritable();

	/**
	 * Wait for the specified duration or until Interrupt() gets
	 * called.
	 *
	 * @return false if Interrupt() was called
	 */
	bool Sleep(std::chrono::milliseconds duration) noexcept;

	/**
	 * Write all of the specified data (which is already in the
	 * output format), waiting for room in the PCM buffer if
	 * necessary.
	 *
	 * Throws #AudioOutputInterrupted after Interrupt() has been
	 * called, and std::runtime_error on error.
	 */
	void WriteAll(std::span<const std::byte> src);
};

TinyAlsaOutput::TinyAlsaOutput(const ConfigBlock &block)
	:AudioOutput(0),
	 card(block.GetBlockValue("card", 0U)),
	 device(block.GetBlockValue("device", 0U)),
	 buffer_time(block.GetPositiveValue("buffer_time",
					    DEFAULT_BUFFER_TIME_US)),
	 period_time(block.GetBlockValue("period_time", 0U))
{
}

static bool
tinyalsa_test_default_device()
{
	const PcmParamsPtr params{pcm_params_get(0, 0, PCM_OUT)};
	return params != nullptr;
}

static bool
TestFormat(struct pcm_params &params, enum pcm_format format) noexcept
{
	return format != PCM_FORMAT_INVALID &&
		pcm_params_format_test(&params, format);
}

/**
 * Check whether the specified sample format is supported, and fall
 * back to the packed version if not.
 *
 * @return the supported format or PCM_FORMAT_INVALID
 */
static enum pcm_format
TryFormatOrPacked(struct pcm_params &params, enum pcm_format format,
		  PcmExport::Params &export_params) noexcept
{
	if (TestFormat(params, format)) {
		export_params.pack24 = false;
		return format;
	}

	format = TinyAlsa::PackPcmFormat(format);
	if (TestFormat(params, format)) {
		export_params.pack24 = true;
		return format;
	}

	return PCM_FORMAT_INVALID;
}

/**
 * Check whether the specified sample format is supported, and try
 * the reversed byte order if not.
 *
 * @return the supported format or PCM_FORMAT_INVALID
 */
static enum pcm_format
TryFormatOrByteSwap(struct pcm_params &params, enum pcm_format format,
		    PcmExport::Params &export_params) noexcept
{
	if (const auto result = TryFormatOrPacked(params, format,
						  export_params);
	    result != PCM_FORMAT_INVALID) {
		export_params.reverse_endian = false;
		return result;
	}

	format = TinyAlsa::ByteSwapPcmFormat(format);
	if (format == PCM_FORMAT_INVALID)
		return PCM_FORMAT_INVALID;

	if (const auto result = TryFormatOrPacked(params, format,
						  export_params);
	    result != PCM_FORMAT_INVALID) {
		export_params.reverse_endian = true;
		return result;
	}

	return PCM_FORMAT_INVALID;
}

/**
 * Choose a sample format, and probe other formats if the specified
 * one is not supported.
 *
 * @return the tinyalsa format or PCM_FORMAT_INVALID if no supported
 * format was found
 */
static enum pcm_format
SetupSampleFormat(struct pcm_params &params, SampleFormat &sample_format,
		  PcmExport::Params &export_params) noexcept
{
	/* try the input format first */

	if (const auto result =
	    TryFormatOrByteSwap(params, TinyAlsa::ToPcmFormat(sample_format),
				export_params);
	    result != PCM_FORMAT_INVALID)
		return result;

	/* if unsupported by the hardware, try other formats */

	static constexpr SampleFormat probe_formats[] = {
		SampleFormat::S24_P32,
		SampleFormat::S32,
		SampleFormat::S16,
		SampleFormat::S8,
	};

	for (const SampleFormat i : probe_formats) {
		if (i == sample_format)
			continue;

		if (const auto result =
		    TryFormatOrByteSwap(params, TinyAlsa::ToPcmFormat(i),
					export_params);
		    result != PCM_FORMAT_INVALID) {
			sample_format = i;
			return result;
		}
	}

	return PCM_FORMAT_INVALID;
}

/**
 * Like std::clamp(), but doesn't require min<=max (the minimum wins).
 */
static constexpr unsigned
Clamp(unsigned value, unsigned min, unsigned max) noexcept
{
	return std::max(std::min(value, max), min);
}

/**
 * Clamp the value to the range supported by the PCM device.
 */
static unsigned
ClampParam(const struct pcm_params &params, enum pcm_param param,
	   unsigned value) noexcept
{
	return Clamp(value, pcm_params_get_min(&params, param),
		     pcm_params_get_max(&params, param));
}

void
TinyAlsaOutput::Open(AudioFormat &audio_format)
{
	interrupted.store(false);
	interrupt_event.Read();

	const PcmParamsPtr params{pcm_params_get(card, device, PCM_OUT)};
	if (!params)
		throw FmtRuntimeError("Failed to open tinyalsa PCM card {} device {}",
				      card, device);

	PcmExport::Params export_params;

	/* tinyalsa has no channel map API; assume ALSA's default
	   channel order */
	export_params.alsa_channel_order = true;

	struct pcm_config config{};

	config.format = SetupSampleFormat(*params, audio_format.format,
					  export_params);
	if (config.format == PCM_FORMAT_INVALID)
		throw FmtRuntimeError("No supported sample format on tinyalsa PCM card {} device {}",
				      card, device);

	config.channels = ClampParam(*params, PCM_PARAM_CHANNELS,
				     audio_format.channels);
	if (!audio_valid_channel_count(config.channels))
		throw FmtRuntimeError("Unsupported number of channels on tinyalsa PCM card {} device {}: {}",
				      card, device, config.channels);

	config.rate = ClampParam(*params, PCM_PARAM_RATE,
				 audio_format.sample_rate);
	if (!audio_valid_sample_rate(config.rate))
		throw FmtRuntimeError("Unsupported sample rate on tinyalsa PCM card {} device {}: {}",
				      card, device, config.rate);

	audio_format.channels = config.channels;
	audio_format.sample_rate = sample_rate = config.rate;

	/* the buffer and period sizes may also be limited by the
	   number of bytes */
	const unsigned frame_bytes =
		pcm_format_to_bits(config.format) / 8 * config.channels;
	const unsigned max_buffer_frames =
		std::min(pcm_params_get_max(params.get(), PCM_PARAM_BUFFER_SIZE),
			 pcm_params_get_max(params.get(), PCM_PARAM_BUFFER_BYTES) / frame_bytes);
	const unsigned max_period_frames =
		std::min(pcm_params_get_max(params.get(), PCM_PARAM_PERIOD_SIZE),
			 pcm_params_get_max(params.get(), PCM_PARAM_PERIOD_BYTES) / frame_bytes);

	const unsigned buffer_frames =
		std::min<unsigned>(audio_format.TimeToFrames(buffer_time),
				   max_buffer_frames);

	period_frames = period_time.count() > 0
		? audio_format.TimeToFrames(period_time)
		: buffer_frames / 4;
	period_frames = Clamp(period_frames,
			      std::max(pcm_params_get_min(params.get(),
							  PCM_PARAM_PERIOD_SIZE),
				       1U),
			      max_period_frames);

	config.period_size = period_frames;
	config.period_count = ClampParam(*params, PCM_PARAM_PERIODS,
					 std::max(buffer_frames / period_frames,
						  2U));

	FmtDebug(tinyalsa_output_domain,
		 "opening card {} device {}: {} period_size={} period_count={}",
		 card, device, audio_format,
		 config.period_size, config.period_count);

	pcm = pcm_open(card, device, PCM_OUT|PCM_NONBLOCK, &config);
	if (!pcm_is_ready(pcm)) {
		auto error = FmtRuntimeError("Failed to open tinyalsa PCM card {} device {}: {}",
					     card, device, pcm_get_error(pcm));
		pcm_close(pcm);
		throw error;
	}

	pcm_export.Open(audio_format.format, audio_format.channels,
			export_params);

	in_frame_size = audio_format.GetFrameSize();
	out_frame_size = pcm_export.GetOutputFrameSize();

	/* all sample formats supported by this plugin have zero
	   silence samples */
	silence = std::make_unique<std::byte[]>(period_frames * out_frame_size);

	written = false;
}

void
TinyAlsaOutput::Close() noexcept
{
	pcm_close(pcm);
	silence.reset();
}

void
TinyAlsaOutput::Interrupt() noexcept
{
	/* this method is called from another thread, so it must only
	   access fields which live as long as this object */
	interrupted.store(true);
	interrupt_event.Write();
}

inline void
TinyAlsaOutput::WaitWritable()
{
	std::array<struct pollfd, 2> pfds{{
		{pcm_get_poll_fd(pcm), POLLOUT, 0},
		{interrupt_event.Get().Get(), POLLIN, 0},
	}};

	while (true) {
		if (interrupted.load())
			/* a command is pending - don't block */
			throw AudioOutputInterrupted{};

		if (poll(pfds.data(), pfds.size(), -1) < 0) {
			if (errno == EINTR)
				continue;

			throw MakeErrno("poll() failed");
		}

		if (pfds[0].revents != 0)
			/* writable (or POLLERR, e.g. after an xrun,
			   which the next pcm_writei() call recovers
			   from) */
			return;

		if (pfds[1].revents != 0)
			/* consume the event; the "interrupted" flag
			   is checked at the beginning of the loop */
			interrupt_event.Read();
	}
}

inline bool
TinyAlsaOutput::Sleep(std::chrono::milliseconds duration) noexcept
{
	if (interrupted.load())
		return false;

	struct pollfd pfd{interrupt_event.Get().Get(), POLLIN, 0};
	(void)poll(&pfd, 1, duration.count());

	return !interrupted.load();
}

void
TinyAlsaOutput::WriteAll(std::span<const std::byte> src)
{
	assert(src.size() % out_frame_size == 0);

	while (!src.empty()) {
		const int n = pcm_writei(pcm, src.data(),
					 src.size() / out_frame_size);
		if (n < 0) {
			if (errno != EAGAIN)
				throw FmtRuntimeError("Failed to write to tinyalsa PCM card {} device {}: {}",
						      card, device, pcm_get_error(pcm));

			WaitWritable();
			continue;
		}

		src = src.subspan(n * out_frame_size);
		written = true;
	}
}

std::size_t
TinyAlsaOutput::Play(std::span<const std::byte> src)
{
	assert(!src.empty());
	assert(src.size() % in_frame_size == 0);

	/* wait until there is room for at least one period */
	WaitWritable();

	/* write no more than one period, so the pcm_writei() call
	   in WriteAll() doesn't need to wait (PcmExport has already
	   consumed the input, so all of its output must be
	   written) */
	const std::size_t max_size = period_frames * in_frame_size;
	if (src.size() > max_size)
		src = src.first(max_size);

	if (const auto e = pcm_export.Export(src); !e.empty())
		WriteAll(e);

	return src.size();
}

void
TinyAlsaOutput::Drain()
{
	/* the stream is discontinuous now; discard the incomplete
	   block which may still be inside the PcmExport instance,
	   because it would otherwise be prepended to the next song */
	pcm_export.Reset();

	if (!written)
		/* nothing to drain */
		return;

	/* tinyalsa 2.0 has no pcm_drain(); instead, wait until the
	   remaining data has been played, and append one period of
	   silence, so the device doesn't play stale data from its
	   ring buffer until we stop it */

	try {
		WriteAll({silence.get(), period_frames * out_frame_size});
	} catch (AudioOutputInterrupted) {
		/* a command is pending; stop draining (this method
		   must not throw AudioOutputInterrupted) */
		return;
	}

	/* the start threshold may not have been reached yet (e.g. if
	   the song was very short) */
	pcm_start(pcm);

	while (true) {
		const long delay = pcm_get_delay(pcm);
		if (delay <= (long)period_frames)
			/* all data except for the silence has been
			   played (or an xrun has occurred) */
			break;

		const auto duration =
			PcmFramesToTime<std::chrono::milliseconds>(delay - period_frames,
								   sample_rate);
		if (!Sleep(duration + std::chrono::milliseconds{1}))
			/* a command is pending; stop draining */
			break;
	}

	pcm_stop(pcm);
	written = false;
}

void
TinyAlsaOutput::Cancel() noexcept
{
	interrupted.store(false);
	interrupt_event.Read();

	pcm_stop(pcm);
	pcm_export.Reset();
	written = false;
}

constexpr struct AudioOutputPlugin tinyalsa_output_plugin = {
	"tinyalsa",
	tinyalsa_test_default_device,
	&TinyAlsaOutput::Create,
	&tinyalsa_mixer_plugin,
};
