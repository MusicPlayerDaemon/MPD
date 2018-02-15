#include "config.h"
#include "YtdlPlaylistPlugin.hxx"
#include "../PlaylistPlugin.hxx"
#include "../MemorySongEnumerator.hxx"
#include "lib/ytdl/YtdlParser.hxx"
#include "config/Block.hxx"
#include "tag/TagBuilder.hxx"
#include "tag/Tag.hxx"
#include "util/Alloc.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <string>

#include <string.h>
#include <stdlib.h>

static constexpr Domain ytdl_domain("youtube-dl");

static bool
ytdl_init(const ConfigBlock &block)
{
	return true;
}

static SongEnumerator *
ytdl_open_uri(const char *uri, Mutex &mutex, Cond &cond)
{
	if (!strncmp(uri, "ytdl://", 7)) {
		uri += 7;
		YTDL_PLAYLIST_MODE playlist_mode = YTDL_PLAYLIST_MODE_SINGLE;
		if (!strncmp(uri, "list/", 5)) {
			uri += 5;
			playlist_mode = YTDL_PLAYLIST_MODE_FULL;
		} else if (!strncmp(uri, "flat/", 5)) {
			uri += 5;
			playlist_mode = YTDL_PLAYLIST_MODE_FLAT;
		}

		// TODO: uri prefix to allow adding URLs directly rather than
		// generating youtube-dl:// URIs, since some hosting sites probably
		// don't generate expiring URLs. Perhaps we could figure out a whitelist
		// based on the extractor key?

		YtdlParseContext context;
		if (YtdlParseJson(&context, uri, playlist_mode)) {
			if (context.extractor.empty()) {
				context.extractor = "youtube-dl";
			}

			context.builder->AddItem(TAG_COMMENT, context.webpage_url.c_str());
			context.builder->AddItem(TAG_ALBUM, context.extractor.c_str());
			Tag playlist(context.builder->Commit());

			std::forward_list<DetachedSong> songs;
			if (context.entries.empty()) {
				std::string url(uri /* context.webpage_url */);
				url.insert(0, "youtube-dl://notag/");
				songs.emplace_front(url.c_str(), std::move(playlist));
			} else {
				context.entries.sort<YtdlPlaylistSort>(YtdlPlaylistSort());
				for (auto iter = context.entries.begin(); iter != context.entries.end(); iter++) {
					if (!iter->builder->HasType(TAG_ALBUM)) {
						iter->builder->AddItem(TAG_ALBUM, playlist.GetValue(TAG_TITLE));
					}
					if (!iter->builder->HasType(TAG_ARTIST)) {
						iter->builder->AddItem(TAG_ARTIST, playlist.GetValue(TAG_ARTIST));
					}
					if (!context.webpage_url.empty()) {
						iter->builder->AddItem(TAG_COMMENT, context.webpage_url.c_str());
					}
					if (!iter->webpage_url.empty()) {
						iter->builder->AddItem(TAG_COMMENT, iter->webpage_url.c_str());
					}
					std::string& url = iter->webpage_url.empty() ? iter->url : iter->webpage_url;
					if (iter->type == "url") {
						if (context.extractor == "YoutubePlaylist") {
							url.insert(0, "https://www.youtube.com/watch?v=");
						}
						iter->builder->AddItem(TAG_COMMENT, url.c_str());
						url.insert(0, "youtube-dl://");
					} else {
						url.insert(0, "youtube-dl://notag/");
					}
					songs.emplace_front(url.c_str(), iter->builder->Commit());
				}
				songs.reverse();
			}

			return new MemorySongEnumerator(std::move(songs));
		}
	}

	return nullptr;
}

static const char *const ytdl_schemes[] = {
	"ytdl",
	nullptr
};

const struct playlist_plugin ytdl_playlist_plugin = {
	"youtube-dl",

	ytdl_init,
	nullptr,
	ytdl_open_uri,
	nullptr,

	ytdl_schemes,
	nullptr,
	nullptr,
};
