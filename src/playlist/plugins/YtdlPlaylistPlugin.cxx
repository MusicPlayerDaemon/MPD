#include "config.h"
#include "YtdlPlaylistPlugin.hxx"
#include "../PlaylistPlugin.hxx"
#include "../MemorySongEnumerator.hxx"
#include "lib/ytdl/Init.hxx"
#include "lib/ytdl/Parser.hxx"
#include "lib/ytdl/TagHandler.hxx"
#include "tag/Builder.hxx"
#include "tag/Tag.hxx"

static Ytdl::YtdlInit *ytdl_init;

static bool
playlist_ytdl_init(const ConfigBlock &block)
{
	ytdl_init = new Ytdl::YtdlInit();

	ytdl_init->Init(block);

	return true;
}

static void
playlist_ytdl_finish() noexcept
{
	delete ytdl_init;
}

static const char *const playlist_ytdl_schemes[] = {
	"ytdl",
	"http",
	"https",
	nullptr
};

static std::unique_ptr<SongEnumerator>
playlist_ytdl_open_uri(const char *uri, Mutex &mutex)
{
	uri = ytdl_init->UriSupported(uri);
	if (!uri) {
		return nullptr;
	}

	Ytdl::TagHandler metadata;
	Ytdl::Parser parser(metadata);
	auto handle = parser.CreateHandle();
	Ytdl::BlockingInvoke(*handle, uri, Ytdl::PlaylistMode::FLAT);

	// TODO: sites that don't have expiring URLs don't need the input plugin

	Tag playlist(metadata.GetTagBuilder().Commit());

	std::forward_list<DetachedSong> songs;
	if (metadata.GetEntries().empty()) {
		std::string url(uri /* metadata.GetWebpageURL() */);
		url.insert(0, "ytdl://");
		songs.emplace_front(url.c_str(), std::move(playlist));
	} else {
		for (auto &entry : metadata.GetEntries()) {
			std::string url = entry.GetWebpageURL().empty()
				? entry.GetURL() : entry.GetWebpageURL();
			url.insert(0, "ytdl://");
			songs.emplace_front(url.c_str(), entry.GetTagBuilder().Commit());
		}
		songs.reverse();
	}

	return std::make_unique<MemorySongEnumerator>(std::move(songs));
}

const PlaylistPlugin ytdl_playlist_plugin =
	PlaylistPlugin("youtube-dl", playlist_ytdl_open_uri)
	.WithInit(playlist_ytdl_init, playlist_ytdl_finish)
	.WithSchemes(playlist_ytdl_schemes);
