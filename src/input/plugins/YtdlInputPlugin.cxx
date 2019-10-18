#include "config.h"
#include "lib/ytdl/Parser.hxx"
#include "lib/ytdl/TagHandler.hxx"
#include "lib/ytdl/Init.hxx"
#include "util/StringAPI.hxx"
#include "util/UriUtil.hxx"
#include "tag/Tag.hxx"
#include "config/Block.hxx"
#include "YtdlInputPlugin.hxx"
#include "YtdlInputStream.hxx"
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

static bool
input_ytdl_supports_uri(const char *uri) noexcept
{
	return ytdl_init->UriSupported(uri) != nullptr;
}

static InputStreamPtr
input_ytdl_open(const char *uri, Mutex &mutex)
{
	uri = ytdl_init->UriSupported(uri);
	if (uri) {
		return std::make_unique<YtdlInputStream>(uri, mutex, ytdl_init->GetEventLoop());
	}

	return nullptr;
}

static std::set<std::string> input_ytdl_protocols()
{
	std::set<std::string> protocols;
	protocols.emplace("ytdl");
	return protocols;
}

static std::unique_ptr<RemoteTagScanner>
input_ytdl_scan_tags(const char* uri, RemoteTagHandler &handler)
{
	uri = ytdl_init->UriSupported(uri);
	if (uri) {
		return std::make_unique<YtdlTagScanner>(ytdl_init->GetEventLoop(), uri, handler);
	}

	return nullptr;
}

const struct InputPlugin input_plugin_ytdl = {
	"youtube-dl",
	nullptr,
	input_ytdl_init,
	input_ytdl_finish,
	input_ytdl_open,
	input_ytdl_protocols,
	input_ytdl_scan_tags,
	input_ytdl_supports_uri,
};
