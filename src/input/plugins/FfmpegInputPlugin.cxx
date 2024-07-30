// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/* necessary because libavutil/common.h uses UINT64_C */
#define __STDC_CONSTANT_MACROS

#include "FfmpegInputPlugin.hxx"
#include "lib/ffmpeg/IOContext.hxx"
#include "lib/ffmpeg/Init.hxx"
#include "../ThreadInputStream.hxx"
#include "PluginUnavailable.hxx"
#include "../InputPlugin.hxx"
#include "util/StringAPI.hxx"

class FfmpegInputStream final : public ThreadInputStream {
	static constexpr std::size_t BUFFER_SIZE = 256 * 1024;

	Ffmpeg::IOContext io;

public:
	FfmpegInputStream(const char *_uri, Mutex &_mutex)
		:ThreadInputStream("ffmpeg", _uri, _mutex, BUFFER_SIZE)
	{
		Start();
	}

	~FfmpegInputStream() noexcept override {
		Stop();
	}

	/* virtual methods from ThreadInputStream */
	void Open() override;
	std::size_t ThreadRead(std::span<std::byte> dest) override;
	void ThreadSeek(offset_type offset) override;

	void Close() noexcept override {
		io = {};
	}
};

[[gnu::const]]
static inline bool
input_ffmpeg_supported() noexcept
{
	void *opaque = nullptr;
	return avio_enum_protocols(&opaque, 0) != nullptr;
}

static void
input_ffmpeg_init(EventLoop &, const ConfigBlock &)
{
	FfmpegInit();

	/* disable this plugin if there's no registered protocol */
	if (!input_ffmpeg_supported())
		throw PluginUnavailable("No protocol");
}

static std::set<std::string, std::less<>>
input_ffmpeg_protocols() noexcept
{
	void *opaque = nullptr;
	const char* protocol;
	std::set<std::string, std::less<>> protocols;
	while ((protocol = avio_enum_protocols(&opaque, 0))) {
		if (StringIsEqual(protocol, "hls")) {
			/* just "hls://" doesn't work, but these do
			   work: */
			protocols.emplace("hls+http://");
			protocols.emplace("hls+https://");
			continue;
		}

		if (protocol_is_whitelisted(protocol)) {
			std::string schema(protocol);
			schema.append("://");
			protocols.emplace(schema);
		}
	}

	return protocols;
}

static InputStreamPtr
input_ffmpeg_open(const char *uri,
		  Mutex &mutex)
{
	return std::make_unique<FfmpegInputStream>(uri, mutex);
}

void
FfmpegInputStream::Open()
{
	io = {GetURI(), AVIO_FLAG_READ};

	seekable = (io->seekable & AVIO_SEEKABLE_NORMAL) != 0;
	size = io.GetSize();

	/* hack to make MPD select the "ffmpeg" decoder plugin - since
	   avio.h doesn't tell us the MIME type of the resource, we
	   can't select a decoder plugin, but the "ffmpeg" plugin is
	   quite good at auto-detection */
	SetMimeType("audio/x-mpd-ffmpeg");
}

std::size_t
FfmpegInputStream::ThreadRead(std::span<std::byte> dest)
{
	return io.Read(dest);
}

void
FfmpegInputStream::ThreadSeek(offset_type new_offset)
{
	io.Seek(new_offset);
}

const InputPlugin input_plugin_ffmpeg = {
	"ffmpeg",
	nullptr,
	input_ffmpeg_init,
	nullptr,
	input_ffmpeg_open,
	input_ffmpeg_protocols
};
