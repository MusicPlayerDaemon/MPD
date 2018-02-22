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
	"youtube-dl",
	"http",
	"https",
	nullptr
};

std::unique_ptr<SongEnumerator>
playlist_ytdl_open_uri(const char *uri, Mutex &mutex, Cond &cond)
{
	uri = ytdl_init->UriSupported(uri);
	if (!uri) {
		return nullptr;
	}

	Ytdl::TagHandler metadata;
	Ytdl::Parser parser(metadata);
	auto handle = parser.CreateHandle();
	Ytdl::Invoke(*handle, uri, Ytdl::PlaylistMode::FLAT);

	// TODO: sites that don't have expiring URLs don't need the input plugin

	if (metadata.extractor.empty()) {
		metadata.extractor = "youtube-dl";
	}

	metadata.builder->AddItem(TAG_COMMENT, metadata.webpage_url.c_str());
	metadata.builder->AddItem(TAG_ALBUM, metadata.extractor.c_str());
	Tag playlist(metadata.builder->Commit());

	std::forward_list<DetachedSong> songs;
	if (metadata.entries.empty()) {
		std::string url(uri /* metadata.webpage_url */);
		url.insert(0, "youtube-dl://");
		songs.emplace_front(url.c_str(), std::move(playlist));
	} else {
		metadata.SortEntries();
		for (auto &entry : metadata.entries) {
			if (!entry.builder->HasType(TAG_ALBUM)) {
				entry.builder->AddItem(TAG_ALBUM, playlist.GetValue(TAG_TITLE));
			}
			if (!entry.builder->HasType(TAG_ARTIST)) {
				entry.builder->AddItem(TAG_ARTIST, playlist.GetValue(TAG_ARTIST));
			}
			if (!metadata.webpage_url.empty()) {
				entry.builder->AddItem(TAG_COMMENT, metadata.webpage_url.c_str());
			}
			if (!entry.webpage_url.empty()) {
				entry.builder->AddItem(TAG_COMMENT, entry.webpage_url.c_str());
			}
			std::string& url = entry.webpage_url.empty() ? entry.url : entry.webpage_url;
			if (entry.type == "url") {
				if (metadata.extractor == "YoutubePlaylist") {
					url.insert(0, "https://www.youtube.com/watch?v=");
				}
				entry.builder->AddItem(TAG_COMMENT, url.c_str());
			}
			url.insert(0, "youtube-dl://");
			songs.emplace_front(url.c_str(), entry.builder->Commit());
		}
		songs.reverse();
	}

	return std::make_unique<MemorySongEnumerator>(std::move(songs));
}

const struct playlist_plugin ytdl_playlist_plugin = {
	"youtube-dl",

	playlist_ytdl_init,
	playlist_ytdl_finish,
	playlist_ytdl_open_uri,
	nullptr,

	playlist_ytdl_schemes,
	nullptr,
	nullptr,
};
