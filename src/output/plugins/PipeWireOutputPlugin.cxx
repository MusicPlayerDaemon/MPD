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
//#include "lib/pipewire/MainLoop.hxx"
#include "../OutputAPI.hxx"
#include "../Error.hxx"
#include "thread/Thread.hxx"

#ifdef __GNUC__
#pragma GCC diagnostic push
/* oh no, libspa likes to cast away "const"! */
#pragma GCC diagnostic ignored "-Wcast-qual"
/* suppress more annoying warnings */
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include <boost/lockfree/spsc_queue.hpp>

#include <stdexcept>

class PipeWireOutput final : AudioOutput {
	Thread thread{BIND_THIS_METHOD(RunThread)};
	struct pw_main_loop *loop;
	struct pw_stream *stream;

	std::byte buffer[1024];
	struct spa_pod_builder pod_builder;

	std::size_t frame_size;

	boost::lockfree::spsc_queue<std::byte> *ring_buffer;

	const uint32_t target_id;

	volatile bool interrupted;

	explicit PipeWireOutput(const ConfigBlock &block);

public:
	static AudioOutput *Create(EventLoop &,
				   const ConfigBlock &block) {
		pw_init(0, nullptr);

		return new PipeWireOutput(block);
	}

	static constexpr struct pw_stream_events MakeStreamEvents() noexcept {
		struct pw_stream_events events{};
		events.version = PW_VERSION_STREAM_EVENTS;
		events.process = Process;
		return events;
	}

private:
	void Process() noexcept;

	static void Process(void *data) noexcept {
		auto &o = *(PipeWireOutput *)data;
		o.Process();
	}

	void RunThread() noexcept {
		pw_main_loop_run(loop);
	}

	/* virtual methods from class AudioOutput */
	void Enable() override;
	void Disable() noexcept override;

	void Open(AudioFormat &audio_format) override;
	void Close() noexcept override;

	void Interrupt() noexcept override {
		interrupted = true;
	}

	size_t Play(const void *chunk, size_t size) override;

	// TODO: void Drain() override;
	// TODO: void Cancel() noexcept override;
	// TODO: bool Pause() noexcept override;
};

static constexpr auto stream_events = PipeWireOutput::MakeStreamEvents();

inline
PipeWireOutput::PipeWireOutput(const ConfigBlock &block)
	:AudioOutput(FLAG_ENABLE_DISABLE),
	 target_id(block.GetBlockValue("target", unsigned(PW_ID_ANY)))
{
}

void
PipeWireOutput::Enable()
{
	loop = pw_main_loop_new(nullptr);
	if (loop == nullptr)
		throw std::runtime_error("pw_main_loop_new() failed");

	try {
		thread.Start();
	} catch (...) {
		pw_main_loop_destroy(loop);
		throw;
	}
}

void
PipeWireOutput::Disable() noexcept
{
	pw_main_loop_quit(loop);
	thread.Join();

	pw_main_loop_destroy(loop);
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

	raw.flags |= SPA_AUDIO_FLAG_UNPOSITIONED; // TODO
	// TODO raw.position[]

	return raw;
}

void
PipeWireOutput::Open(AudioFormat &audio_format)
{
	auto props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio",
				       PW_KEY_MEDIA_CATEGORY, "Playback",
				       PW_KEY_MEDIA_ROLE, "Music",
				       PW_KEY_APP_NAME, "Music Player Daemon",
				       PW_KEY_NODE_NAME, "mpd",
				       nullptr);

	stream = pw_stream_new_simple(pw_main_loop_get_loop(loop),
				      "mpd",
				      props,
				      &stream_events,
				      this);
	if (stream == nullptr)
		throw std::runtime_error("pw_stream_new_simple() failed");

	auto raw = ToPipeWireAudioFormat(audio_format);

	frame_size = audio_format.GetFrameSize();
	interrupted = false;

	/* allocate a ring buffer of 1 second */
	ring_buffer = new boost::lockfree::spsc_queue<std::byte>(frame_size *
								 audio_format.sample_rate);

	const struct spa_pod *params[1];

	pod_builder = {};
	pod_builder.data = buffer;
	pod_builder.size = sizeof(buffer);
	params[0] = spa_format_audio_raw_build(&pod_builder,
					       SPA_PARAM_EnumFormat, &raw);

	pw_stream_connect(stream,
			  PW_DIRECTION_OUTPUT,
			  target_id,
			  (enum pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT |
						 PW_STREAM_FLAG_MAP_BUFFERS |
						 PW_STREAM_FLAG_RT_PROCESS),
			  params, 1);
}

void
PipeWireOutput::Close() noexcept
{
	pw_stream_destroy(stream);

	// TODO synchronize with Process()?
	delete ring_buffer;
}

inline void
PipeWireOutput::Process() noexcept
{
	auto *b = pw_stream_dequeue_buffer(stream);
	if (b == nullptr) {
		pw_log_warn("out of buffers: %m");
		return;
	}

	auto *buf = b->buffer;
	std::byte *dest = (std::byte *)buf->datas[0].data;
	if (dest == nullptr)
		return;

	const std::size_t max_frames = buf->datas[0].maxsize / frame_size;
	const std::size_t max_size = max_frames * frame_size;

	size_t nbytes = ring_buffer->pop(dest, max_size);
	if (nbytes == 0) {
		pw_stream_flush(stream, true);
		return;
	}

	buf->datas[0].chunk->offset = 0;
	buf->datas[0].chunk->stride = frame_size;
	buf->datas[0].chunk->size = nbytes;

	pw_stream_queue_buffer(stream, b);
}

size_t
PipeWireOutput::Play(const void *chunk, size_t size)
{
	while (true) {
		std::size_t bytes_written =
			ring_buffer->push((const std::byte *)chunk, size);
		if (bytes_written > 0)
			return bytes_written;

		if (interrupted)
			throw AudioOutputInterrupted{};

		usleep(1000); // TODO
	}

	return size;
}

const struct AudioOutputPlugin pipewire_output_plugin = {
	"pipewire",
	nullptr,
	&PipeWireOutput::Create,
	nullptr,
};
