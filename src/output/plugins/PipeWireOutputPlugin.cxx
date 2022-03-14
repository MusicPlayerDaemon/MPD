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

#include "PipeWireOutputPlugin.hxx"
#include "lib/pipewire/Error.hxx"
#include "lib/pipewire/ThreadLoop.hxx"
#include "../OutputAPI.hxx"
#include "../Error.hxx"
#include "mixer/plugins/PipeWireMixerPlugin.hxx"
#include "pcm/Silence.hxx"
#include "system/Error.hxx"
#include "util/BitReverse.hxx"
#include "util/Domain.hxx"
#include "util/ScopeExit.hxx"
#include "util/StringCompare.hxx"
#include "util/WritableBuffer.hxx"
#include "Log.hxx"
#include "tag/Format.hxx"
#include "config.h" // for ENABLE_DSD

#ifdef __GNUC__
#pragma GCC diagnostic push
/* oh no, libspa likes to cast away "const"! */
#pragma GCC diagnostic ignored "-Wcast-qual"
/* suppress more annoying warnings */
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/props.h>

#include <cmath>

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include <boost/lockfree/spsc_queue.hpp>

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>

static constexpr Domain pipewire_output_domain("pipewire_output");

class PipeWireOutput final : AudioOutput {
	const char *const name;

	const char *const remote;
	const char *const target;

	struct pw_thread_loop *thread_loop = nullptr;
	struct pw_stream *stream;

	std::string error_message;

	std::byte pod_buffer[1024];
	struct spa_pod_builder pod_builder;

	std::size_t frame_size;

	/**
	 * This buffer passes PCM data from Play() to Process().
	 */
	using RingBuffer = boost::lockfree::spsc_queue<std::byte>;
	RingBuffer *ring_buffer;

	uint32_t target_id = PW_ID_ANY;

	float volume = 1.0;

	PipeWireMixer *mixer = nullptr;
	unsigned channels;

	/**
	 * The active sample format, needed for PcmSilence().
	 */
	SampleFormat sample_format;

#if defined(ENABLE_DSD) && defined(SPA_AUDIO_DSD_FLAG_NONE)
	/**
	 * Is the "dsd" setting enabled, i.e. is DSD playback allowed?
	 */
	const bool enable_dsd;

	/**
	 * Are we currently playing in native DSD mode?
	 */
	bool use_dsd;

	/**
	 * Reverse the 8 bits in each DSD byte?  This is necessary if
	 * PipeWire wants LSB (because MPD uses MSB internally).
	 */
	bool dsd_reverse_bits;

	/**
	 * Pack this many bytes of each frame together.  MPD uses 1
	 * internally, and if PipeWire wants more than one
	 * (e.g. because it uses DSD_U32), we need to reorder bytes.
	 */
	uint_least8_t dsd_interleave;
#endif

	bool disconnected;

	/**
	 * Shall the previously known volume be restored as soon as
	 * PW_STREAM_STATE_STREAMING is reached?  This needs to be
	 * done each time after the pw_stream got created, thus this
	 * flag gets set by Open().
	 */
	bool restore_volume;

	bool interrupted;
	bool paused;

	/**
	 * Is the PipeWire stream active, i.e. has
	 * pw_stream_set_active() been called successfully?
	 */
	bool active;

	/**
	 * Has Drain() been called?  This causes Process() to invoke
	 * pw_stream_flush() to drain PipeWire as soon as the
	 * #ring_buffer has been drained.
	 */
	bool drain_requested;

	bool drained;

	explicit PipeWireOutput(const ConfigBlock &block);

public:
	static AudioOutput *Create(EventLoop &,
				   const ConfigBlock &block) {
		pw_init(nullptr, nullptr);

		return new PipeWireOutput(block);
	}

	static constexpr struct pw_stream_events MakeStreamEvents() noexcept {
		struct pw_stream_events events{};
		events.version = PW_VERSION_STREAM_EVENTS;
		events.state_changed = StateChanged;
		events.process = Process;
		events.drained = Drained;
		events.control_info = ControlInfo;
		events.param_changed = ParamChanged;
		return events;
	}

	void SetVolume(float volume);

	void SetMixer(PipeWireMixer &_mixer) noexcept;

	void ClearMixer([[maybe_unused]] PipeWireMixer &old_mixer) noexcept {
		assert(mixer == &old_mixer);

		mixer = nullptr;
	}

private:
	void CheckThrowError() {
		if (disconnected) {
			if (error_message.empty())
				throw std::runtime_error("Disconnected from PipeWire");
			else
				throw std::runtime_error(error_message);
		}
	}

	void StateChanged(enum pw_stream_state state,
			  const char *error) noexcept;

	static void StateChanged(void *data,
				 [[maybe_unused]] enum pw_stream_state old,
				 enum pw_stream_state state,
				 const char *error) noexcept {
		auto &o = *(PipeWireOutput *)data;
		o.StateChanged(state, error);
	}

	void Process() noexcept;

	static void Process(void *data) noexcept {
		auto &o = *(PipeWireOutput *)data;
		o.Process();
	}

	void Drained() noexcept {
		drained = true;
		pw_thread_loop_signal(thread_loop, false);
	}

	static void Drained(void *data) noexcept {
		auto &o = *(PipeWireOutput *)data;
		o.Drained();
	}

	void ControlInfo(const struct pw_stream_control *control) noexcept {
		float sum = 0;
		unsigned c;
		for (c = 0; c < control->n_values; c++)
			sum += control->values[c];

		sum /= control->n_values;

		if (mixer != nullptr)
			pipewire_mixer_on_change(*mixer, std::cbrt(sum));

		pw_thread_loop_signal(thread_loop, false);
	}

	static void ControlInfo(void *data,
				[[maybe_unused]] uint32_t id,
				const struct pw_stream_control *control) noexcept {
		auto &o = *(PipeWireOutput *)data;
		if (StringIsEqual(control->name, "Channel Volumes"))
			o.ControlInfo(control);
	}

#if defined(ENABLE_DSD) && defined(SPA_AUDIO_DSD_FLAG_NONE)
	void DsdFormatChanged(const struct spa_audio_info_dsd &dsd) noexcept;
	void DsdFormatChanged(const struct spa_pod &param) noexcept;
#endif

	void ParamChanged(uint32_t id, const struct spa_pod *param) noexcept;

	static void ParamChanged(void *data,
				 uint32_t id,
				 const struct spa_pod *param) noexcept
	{
		if (id != SPA_PARAM_Format || param == nullptr)
			return;

		auto &o = *(PipeWireOutput *)data;
		o.ParamChanged(id, param);
	}

	/* virtual methods from class AudioOutput */
	void Enable() override;
	void Disable() noexcept override;

	void Open(AudioFormat &audio_format) override;
	void Close() noexcept override;

	void Interrupt() noexcept override {
		if (thread_loop == nullptr)
			return;

		const PipeWire::ThreadLoopLock lock(thread_loop);
		interrupted = true;
		pw_thread_loop_signal(thread_loop, false);
	}

	[[nodiscard]] std::chrono::steady_clock::duration Delay() const noexcept override;
	size_t Play(const void *chunk, size_t size) override;

	void Drain() override;
	void Cancel() noexcept override;
	bool Pause() noexcept override;

	void SendTag(const Tag &tag) override;
};

static constexpr auto stream_events = PipeWireOutput::MakeStreamEvents();

inline
PipeWireOutput::PipeWireOutput(const ConfigBlock &block)
	:AudioOutput(FLAG_ENABLE_DISABLE),
	 name(block.GetBlockValue("name", "pipewire")),
	 remote(block.GetBlockValue("remote", nullptr)),
	 target(block.GetBlockValue("target", nullptr))
#if defined(ENABLE_DSD) && defined(SPA_AUDIO_DSD_FLAG_NONE)
	, enable_dsd(block.GetBlockValue("dsd", false))
#endif
{
	if (target != nullptr) {
		if (StringIsEmpty(target))
			throw std::runtime_error("target must not be empty");

		char *endptr;
		const auto _target_id = strtoul(target, &endptr, 10);
		if (endptr > target && *endptr == 0)
			/* numeric value means target_id, not target
			   name */
			target_id = (uint32_t)_target_id;
	}
}

void
PipeWireOutput::SetVolume(float _volume)
{
	const PipeWire::ThreadLoopLock lock(thread_loop);

	float newvol = _volume*_volume*_volume;

	if (stream != nullptr && !restore_volume) {
		float vol[MAX_CHANNELS];
		std::fill_n(vol, channels, newvol);

		if (pw_stream_set_control(stream,
				  SPA_PROP_channelVolumes, channels, vol,
				  0) != 0)
			throw std::runtime_error("pw_stream_set_control() failed");
	}

	volume = _volume;
}

void
PipeWireOutput::Enable()
{
	thread_loop = pw_thread_loop_new(name, nullptr);
	if (thread_loop == nullptr)
		throw MakeErrno("pw_thread_loop_new() failed");

	pw_thread_loop_start(thread_loop);

	stream = nullptr;
}

void
PipeWireOutput::Disable() noexcept
{
	pw_thread_loop_destroy(thread_loop);
	thread_loop = nullptr;
}

static constexpr enum spa_audio_format
ToPipeWireSampleFormat(SampleFormat format) noexcept
{
	switch (format) {
	case SampleFormat::UNDEFINED:
		break;

	case SampleFormat::S8:
		return SPA_AUDIO_FORMAT_S8;

	case SampleFormat::S16:
		return SPA_AUDIO_FORMAT_S16;

	case SampleFormat::S24_P32:
		return SPA_AUDIO_FORMAT_S24_32;

	case SampleFormat::S32:
		return SPA_AUDIO_FORMAT_S32;

	case SampleFormat::FLOAT:
		return SPA_AUDIO_FORMAT_F32;

	case SampleFormat::DSD:
		break;
	}

	return SPA_AUDIO_FORMAT_UNKNOWN;
}

static struct spa_audio_info_raw
ToPipeWireAudioFormat(AudioFormat &audio_format) noexcept
{
	struct spa_audio_info_raw raw{};

	raw.format = ToPipeWireSampleFormat(audio_format.format);
	if (raw.format == SPA_AUDIO_FORMAT_UNKNOWN) {
		raw.format = SPA_AUDIO_FORMAT_S16;
		audio_format.format = SampleFormat::S16;
	}

	raw.flags = SPA_AUDIO_FLAG_NONE;
	raw.rate = audio_format.sample_rate;
	raw.channels = audio_format.channels;

	/* MPD uses the FLAC channel assignment
	   (https://xiph.org/flac/format.html) */
	switch (audio_format.channels) {
	case 1:
		raw.position[0] = SPA_AUDIO_CHANNEL_MONO;
		break;

	case 2:
		raw.position[0] = SPA_AUDIO_CHANNEL_FL;
		raw.position[1] = SPA_AUDIO_CHANNEL_FR;
		break;

	case 3:
		raw.position[0] = SPA_AUDIO_CHANNEL_FL;
		raw.position[1] = SPA_AUDIO_CHANNEL_FR;
		raw.position[2] = SPA_AUDIO_CHANNEL_FC;
		break;

	case 4:
		raw.position[0] = SPA_AUDIO_CHANNEL_FL;
		raw.position[1] = SPA_AUDIO_CHANNEL_FR;
		raw.position[2] = SPA_AUDIO_CHANNEL_RL;
		raw.position[3] = SPA_AUDIO_CHANNEL_RR;
		break;

	case 5:
		raw.position[0] = SPA_AUDIO_CHANNEL_FL;
		raw.position[1] = SPA_AUDIO_CHANNEL_FR;
		raw.position[2] = SPA_AUDIO_CHANNEL_FC;
		raw.position[3] = SPA_AUDIO_CHANNEL_RL;
		raw.position[4] = SPA_AUDIO_CHANNEL_RR;
		break;

	case 6:
		raw.position[0] = SPA_AUDIO_CHANNEL_FL;
		raw.position[1] = SPA_AUDIO_CHANNEL_FR;
		raw.position[2] = SPA_AUDIO_CHANNEL_FC;
		raw.position[3] = SPA_AUDIO_CHANNEL_LFE;
		raw.position[4] = SPA_AUDIO_CHANNEL_RL;
		raw.position[5] = SPA_AUDIO_CHANNEL_RR;
		break;

	case 7:
		raw.position[0] = SPA_AUDIO_CHANNEL_FL;
		raw.position[1] = SPA_AUDIO_CHANNEL_FR;
		raw.position[2] = SPA_AUDIO_CHANNEL_FC;
		raw.position[3] = SPA_AUDIO_CHANNEL_LFE;
		raw.position[4] = SPA_AUDIO_CHANNEL_RC;
		raw.position[5] = SPA_AUDIO_CHANNEL_SL;
		raw.position[6] = SPA_AUDIO_CHANNEL_SR;
		break;

	case 8:
		raw.position[0] = SPA_AUDIO_CHANNEL_FL;
		raw.position[1] = SPA_AUDIO_CHANNEL_FR;
		raw.position[2] = SPA_AUDIO_CHANNEL_FC;
		raw.position[3] = SPA_AUDIO_CHANNEL_LFE;
		raw.position[4] = SPA_AUDIO_CHANNEL_RL;
		raw.position[5] = SPA_AUDIO_CHANNEL_RR;
		raw.position[6] = SPA_AUDIO_CHANNEL_SL;
		raw.position[7] = SPA_AUDIO_CHANNEL_SR;
		break;

	default:
		raw.flags |= SPA_AUDIO_FLAG_UNPOSITIONED;
	}

	return raw;
}

void
PipeWireOutput::Open(AudioFormat &audio_format)
{
	error_message.clear();
	disconnected = false;
	restore_volume = true;

	paused = false;

	/* stay inactive (PW_STREAM_FLAG_INACTIVE) until the ring
	   buffer has been filled */
	active = false;

	drain_requested = false;
	drained = true;

	auto props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio",
				       PW_KEY_MEDIA_CATEGORY, "Playback",
				       PW_KEY_MEDIA_ROLE, "Music",
				       PW_KEY_APP_NAME, "Music Player Daemon",
				       nullptr);

	pw_properties_setf(props, PW_KEY_NODE_NAME, "mpd.%s", name);

	if (remote != nullptr && target_id == PW_ID_ANY)
		pw_properties_setf(props, PW_KEY_REMOTE_NAME, "%s", remote);

	if (target != nullptr && target_id == PW_ID_ANY)
		pw_properties_setf(props, PW_KEY_NODE_TARGET, "%s", target);

#ifdef PW_KEY_NODE_RATE
	/* ask PipeWire to change the graph sample rate to ours
	   (requires PipeWire 0.3.32) */
	pw_properties_setf(props, PW_KEY_NODE_RATE, "1/%u",
			   audio_format.sample_rate);
#endif

	const PipeWire::ThreadLoopLock lock(thread_loop);

	stream = pw_stream_new_simple(pw_thread_loop_get_loop(thread_loop),
				      "mpd",
				      props,
				      &stream_events,
				      this);
	if (stream == nullptr)
		throw MakeErrno("pw_stream_new_simple() failed");

#if defined(ENABLE_DSD) && defined(SPA_AUDIO_DSD_FLAG_NONE)
	/* this needs to be determined before ToPipeWireAudioFormat()
	   switches DSD to S16 */
	use_dsd = enable_dsd &&
		audio_format.format == SampleFormat::DSD;
	dsd_reverse_bits = false;
	dsd_interleave = 0;
#endif

	auto raw = ToPipeWireAudioFormat(audio_format);

#if defined(ENABLE_DSD) && defined(SPA_AUDIO_DSD_FLAG_NONE)
	if (use_dsd)
		/* restore the DSD format which was overwritten by
		   ToPipeWireAudioFormat(), because DSD is a special
		   case in PipeWire */
		audio_format.format = SampleFormat::DSD;
#endif

	frame_size = audio_format.GetFrameSize();
	sample_format = audio_format.format;
	channels = audio_format.channels;
	interrupted = false;

	/* allocate a ring buffer of 0.5 seconds */
	const std::size_t ring_buffer_size =
		frame_size * (audio_format.sample_rate / 2);
	ring_buffer = new RingBuffer(ring_buffer_size);

	const struct spa_pod *params[1];

	pod_builder = {};
	pod_builder.data = pod_buffer;
	pod_builder.size = sizeof(pod_buffer);

#if defined(ENABLE_DSD) && defined(SPA_AUDIO_DSD_FLAG_NONE)
	struct spa_audio_info_dsd dsd;
	if (use_dsd) {
		dsd = {};

		/* copy all relevant settings from the
		   ToPipeWireAudioFormat() return value */
		dsd.flags = raw.flags;
		dsd.rate = raw.rate;
		dsd.channels = raw.channels;
		if ((dsd.flags & SPA_AUDIO_FLAG_UNPOSITIONED) == 0)
			std::copy_n(raw.position, dsd.channels, dsd.position);

		params[0] = spa_format_audio_dsd_build(&pod_builder,
						       SPA_PARAM_EnumFormat,
						       &dsd);
	} else
#endif
		params[0] = spa_format_audio_raw_build(&pod_builder,
						       SPA_PARAM_EnumFormat,
						       &raw);

	int error =
		pw_stream_connect(stream,
				  PW_DIRECTION_OUTPUT,
				  target_id,
				  (enum pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT |
							 PW_STREAM_FLAG_INACTIVE |
							 PW_STREAM_FLAG_MAP_BUFFERS |
							 PW_STREAM_FLAG_RT_PROCESS),
				  params, 1);
	if (error < 0)
		throw PipeWire::MakeError(error, "Failed to connect stream");
}

void
PipeWireOutput::Close() noexcept
{
	{
		const PipeWire::ThreadLoopLock lock(thread_loop);
		pw_stream_destroy(stream);
		stream = nullptr;
	}

	delete ring_buffer;
}

inline void
PipeWireOutput::StateChanged(enum pw_stream_state state,
			     [[maybe_unused]] const char *error) noexcept
{
	const bool was_disconnected = disconnected;
	disconnected = state == PW_STREAM_STATE_ERROR ||
		state == PW_STREAM_STATE_UNCONNECTED;
	if (!was_disconnected && disconnected) {
		if (error != nullptr)
			error_message = error;

		pw_thread_loop_signal(thread_loop, false);
	}

}

#if defined(ENABLE_DSD) && defined(SPA_AUDIO_DSD_FLAG_NONE)

inline void
PipeWireOutput::DsdFormatChanged(const struct spa_audio_info_dsd &dsd) noexcept
{
	/* MPD uses MSB internally, which means if PipeWire asks LSB
	   from us, we need to reverse the bits in each DSD byte */
	dsd_reverse_bits = dsd.bitorder == SPA_PARAM_BITORDER_lsb;

	dsd_interleave = dsd.interleave;
}

inline void
PipeWireOutput::DsdFormatChanged(const struct spa_pod &param) noexcept
{
	uint32_t media_type, media_subtype;
	struct spa_audio_info_dsd dsd;

	if (spa_format_parse(&param, &media_type, &media_subtype) >= 0 &&
	    media_type == SPA_MEDIA_TYPE_audio &&
	    media_subtype == SPA_MEDIA_SUBTYPE_dsd &&
	    spa_format_audio_dsd_parse(&param, &dsd) >= 0)
		DsdFormatChanged(dsd);
}

#endif

inline void
PipeWireOutput::ParamChanged([[maybe_unused]] uint32_t id,
			     [[maybe_unused]] const struct spa_pod *param) noexcept
{
	if (restore_volume) {
		SetVolume(volume);
		restore_volume = false;
	}

#if defined(ENABLE_DSD) && defined(SPA_AUDIO_DSD_FLAG_NONE)
	if (use_dsd && id == SPA_PARAM_Format && param != nullptr)
		DsdFormatChanged(*param);
#endif
}

#if defined(ENABLE_DSD) && defined(SPA_AUDIO_DSD_FLAG_NONE)

static void
Interleave(std::byte *data, std::byte *end,
	   std::size_t channels, std::size_t interleave) noexcept
{
	assert(channels > 1);
	assert(channels <= MAX_CHANNELS);

	constexpr std::size_t MAX_INTERLEAVE = 8;
	assert(interleave > 1);
	assert(interleave <= MAX_INTERLEAVE);

	std::array<std::byte, MAX_CHANNELS * MAX_INTERLEAVE> buffer;
	std::size_t buffer_size = channels * interleave;

	while (data < end) {
		std::copy_n(data, buffer_size, buffer.data());

		const std::byte *src0 = buffer.data();
		for (std::size_t channel = 0; channel < channels;
		     ++channel, ++src0) {
			const std::byte *src = src0;
			for (std::size_t i = 0; i < interleave;
			     ++i, src += channels)
				*data++ = *src;
		}
	}
}

static void
BitReverse(uint8_t *data, std::size_t n) noexcept
{
	while (n-- > 0)
		*data = bit_reverse(*data);
}

static void
BitReverse(std::byte *data, std::size_t n) noexcept
{
	BitReverse((uint8_t *)data, n);
}

static void
PostProcessDsd(std::byte *data, struct spa_chunk &chunk, unsigned channels,
	       bool reverse_bits, unsigned interleave) noexcept
{
	assert(chunk.size % channels == 0);

	if (interleave > 1 && channels > 1) {
		assert(chunk.size % (channels * interleave) == 0);

		Interleave(data, data + chunk.size, channels, interleave);
		chunk.stride *= interleave;
	}

	if (reverse_bits)
		BitReverse(data, chunk.size);
}

#endif

inline void
PipeWireOutput::Process() noexcept
{
	auto *b = pw_stream_dequeue_buffer(stream);
	if (b == nullptr) {
		pw_log_warn("out of buffers: %m");
		return;
	}

	auto &buffer = *b->buffer;
	auto &d = buffer.datas[0];

	auto dest = (std::byte *)d.data;
	if (dest == nullptr)
		return;

	std::size_t max_frames = d.maxsize / frame_size;

#if defined(ENABLE_DSD) && defined(SPA_AUDIO_DSD_FLAG_NONE)
	if (use_dsd && dsd_interleave > 1) {
		/* make sure we don't get partial interleave frames */
		std::size_t interleave_size = frame_size * dsd_interleave;
		std::size_t available_bytes = ring_buffer->read_available();
		std::size_t available_interleaves =
			available_bytes / interleave_size;
		std::size_t available_frames =
			available_interleaves * dsd_interleave;
		if (max_frames > available_frames)
			max_frames = available_frames;
	}
#endif

	const std::size_t max_size = max_frames * frame_size;
	size_t nbytes = ring_buffer->pop(dest, max_size);
	assert(nbytes % frame_size == 0);
	if (nbytes == 0) {
		if (drain_requested) {
			pw_stream_flush(stream, true);
			return;
		}

		/* buffer underrun: generate some silence */
		PcmSilence({dest, max_size}, sample_format);
		nbytes = max_size;

		LogWarning(pipewire_output_domain, "Decoder is too slow; playing silence to avoid xrun");
	}

	auto &chunk = *d.chunk;
	chunk.offset = 0;
	chunk.stride = frame_size;
	chunk.size = nbytes;

#if defined(ENABLE_DSD) && defined(SPA_AUDIO_DSD_FLAG_NONE)
	if (use_dsd)
		PostProcessDsd(dest, chunk, channels,
			       dsd_reverse_bits, dsd_interleave);
#endif

	pw_stream_queue_buffer(stream, b);

	pw_thread_loop_signal(thread_loop, false);
}

std::chrono::steady_clock::duration
PipeWireOutput::Delay() const noexcept
{
	const PipeWire::ThreadLoopLock lock(thread_loop);

	auto result = std::chrono::steady_clock::duration::zero();
	if (paused)
		/* idle while paused */
		result = std::chrono::seconds(1);

	return result;
}

size_t
PipeWireOutput::Play(const void *chunk, size_t size)
{
	const PipeWire::ThreadLoopLock lock(thread_loop);

	paused = false;

	while (true) {
		CheckThrowError();

		std::size_t bytes_written =
			ring_buffer->push((const std::byte *)chunk, size);
		if (bytes_written > 0) {
			drained = false;
			return bytes_written;
		}

		if (!active) {
			/* now that the ring_buffer is full, there is
			   enough data for Process(), so let's resume
			   the stream now */
			active = true;
			pw_stream_set_active(stream, true);
		}

		if (interrupted)
			throw AudioOutputInterrupted{};

		pw_thread_loop_wait(thread_loop);
	}
}

void
PipeWireOutput::Drain()
{
	const PipeWire::ThreadLoopLock lock(thread_loop);

	drain_requested = true;
	AtScopeExit(this) { drain_requested = false; };

	while (!drained && !interrupted) {
		CheckThrowError();
		pw_thread_loop_wait(thread_loop);
	}
}

void
PipeWireOutput::Cancel() noexcept
{
	const PipeWire::ThreadLoopLock lock(thread_loop);
	interrupted = false;

	ring_buffer->reset();
}

bool
PipeWireOutput::Pause() noexcept
{
	const PipeWire::ThreadLoopLock lock(thread_loop);
	interrupted = false;

	paused = true;

	if (active) {
		active = false;
		pw_stream_set_active(stream, false);
	}

	return true;
}

inline void
PipeWireOutput::SetMixer(PipeWireMixer &_mixer) noexcept
{
	assert(mixer == nullptr);

	mixer = &_mixer;

	// TODO: Check if context and stream is ready and trigger a volume update...
}

void
PipeWireOutput::SendTag(const Tag &tag)
{
	CheckThrowError();

	struct spa_dict_item items[3];
	uint32_t n_items=0;

	const char *artist, *title;

	char *medianame = FormatTag(tag, "%artist% - %title%");
	AtScopeExit(medianame) { free(medianame); };

	items[n_items++] = SPA_DICT_ITEM_INIT(PW_KEY_MEDIA_NAME, medianame);

	artist = tag.GetValue(TAG_ARTIST);
	title = tag.GetValue(TAG_TITLE);

	if (artist != nullptr) {
		items[n_items++] = SPA_DICT_ITEM_INIT(PW_KEY_MEDIA_ARTIST, artist);
	}

	if (title != nullptr) {
		items[n_items++] = SPA_DICT_ITEM_INIT(PW_KEY_MEDIA_TITLE, title);
	}

	struct spa_dict dict = SPA_DICT_INIT(items, n_items);

	auto rc = pw_stream_update_properties(stream, &dict);
	if (rc < 0)
		LogWarning(pipewire_output_domain, "Error updating properties");
}

void
pipewire_output_set_mixer(PipeWireOutput &po, PipeWireMixer &pm) noexcept
{
	po.SetMixer(pm);
}

void
pipewire_output_clear_mixer(PipeWireOutput &po, PipeWireMixer &pm) noexcept
{
	po.ClearMixer(pm);
}

const struct AudioOutputPlugin pipewire_output_plugin = {
	"pipewire",
	nullptr,
	&PipeWireOutput::Create,
	&pipewire_mixer_plugin,
};

void
pipewire_output_set_volume(PipeWireOutput &output, float volume)
{
	output.SetVolume(volume);
}
