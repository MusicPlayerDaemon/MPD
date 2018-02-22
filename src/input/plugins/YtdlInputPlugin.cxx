#include "config.h"
#include "lib/ytdl/Parser.hxx"
#include "lib/ytdl/TagHandler.hxx"
#include "lib/ytdl/Init.hxx"
#include "util/StringAPI.hxx"
#include "util/UriUtil.hxx"
#include "tag/Tag.hxx"
#include "config/Block.hxx"
#include "YtdlInputPlugin.hxx"
#include "YtdlTagScanner.hxx"
#include "CurlInputPlugin.hxx"
#include "../InputStream.hxx"
#include "../RemoteTagScanner.hxx"
#include "tag/Tag.hxx"
#include <vector>

static Ytdl::YtdlInit *ytdl_init;

static void
input_ytdl_init(EventLoop &event_loop, const ConfigBlock &block)
{
	ytdl_init = new Ytdl::YtdlInit(event_loop);

	ytdl_init->Init(block);
}

static void
input_ytdl_finish() noexcept
{
	delete ytdl_init;
}

static InputStreamPtr
input_ytdl_open(const char *uri, Mutex &mutex, Cond &cond)
{
	uri = ytdl_init->UriSupported(uri);
	if (uri) {
		Ytdl::TagHandler metadata;
		Ytdl::Parser parser(metadata);
		auto handle = parser.CreateHandle();
		Ytdl::Invoke(*handle, uri, Ytdl::PlaylistMode::SINGLE);
		return OpenCurlInputStream(metadata.GetURL().c_str(),
			metadata.GetHeaders(), mutex, cond);
	}

	return nullptr;
}

static std::unique_ptr<RemoteTagScanner>
input_ytdl_scan_tags(const char *uri, RemoteTagHandler &handler)
{
	uri = ytdl_init->UriSupported(uri);
	if (uri) {
		return std::make_unique<YtdlTagScanner>(uri, handler);
	}

	return nullptr;
}

static constexpr const char *ytdl_prefixes[] = {
	"youtube-dl://",
	nullptr
};

const struct InputPlugin input_plugin_ytdl = {
	"youtube-dl",
	ytdl_prefixes,
	input_ytdl_init,
	input_ytdl_finish,
	input_ytdl_open,
	input_ytdl_scan_tags,
};
