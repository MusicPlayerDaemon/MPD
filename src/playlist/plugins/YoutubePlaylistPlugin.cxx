/*
 * Copyright 2003-2020 The Music Player Daemon Project
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

#include "YoutubePlaylistPlugin.hxx"
#include "../PlaylistPlugin.hxx"
#include "../MemorySongEnumerator.hxx"
#include "Log.hxx"
#include "tag/Builder.hxx"
#include "tag/Tag.hxx"
#include "song/DetachedSong.hxx"
#include "util/Alloc.hxx"
#include "util/ScopeExit.hxx"
#include "util/ExecOpen.hxx"
#include "util/Domain.hxx"

#include <yajl/yajl_tree.h>

#include <stdlib.h>
#include <string.h>

#include <string>
#include <forward_list>
#include <utility>

static Domain youtube_domain("youtube");

static bool playlist_youtube_init(const ConfigBlock &block)
{
	return WEXITSTATUS(system("youtube-dl --version > /dev/null")) == 0;
}

static std::unique_ptr<SongEnumerator>
playlist_youtube_open(const char *uri, Mutex &mutex)
{

	const char *args[] = {
		"youtube-dl",
		"--yes-playlist",
		"--ignore-errors",
		"--dump-single-json",
		uri,
		nullptr
	};

	int pid;
	FILE *stream = exec_open(&pid, "youtube-dl", args);
	if(!stream) {
		LogErrno(youtube_domain, "Can't spawn youtube-dl");
		return nullptr;
	}

	/* Read youtube-dl output */
	std::string json;
	char buf[512];
	ssize_t size = 0;
	while(true) {
		size = fread(buf, sizeof(char), 512, stream);
		if(size > 0)
			json.append(buf, size);
		else
			break;
	}

	fclose(stream);
	int status = exec_wait(pid);
	if(status != 0) {
		FormatError(youtube_domain, "youtube-dl returned %d", status);
		return nullptr;
	}

	/* Parse json */
	yajl_val root = yajl_tree_parse(json.c_str(), nullptr, 0);

	if(!root) {
		LogError(youtube_domain, "Failed to parse youtube-dl output");
		return nullptr;
	}

	AtScopeExit(root) {
		yajl_tree_free(root);
	};

	/* Get songs */
	std::forward_list<DetachedSong> songs;
	std::string video_url;

	const char *entries_path[] = { "entries", nullptr };
	auto *entries = YAJL_GET_ARRAY(yajl_tree_get(root, entries_path, yajl_t_array));

	if(!entries) {
		LogError(youtube_domain, "Can't get entries");
		return nullptr;
	}

	for(size_t i = 0; i < entries->len; i++) {
		yajl_val entry = entries->values[i];

		TagBuilder tag_builder;

		/* Get song title */
		{
			const char *path[] = { "title", nullptr };
			char *name = YAJL_GET_STRING(yajl_tree_get(entry, path, yajl_t_string));

			if(name) {
				tag_builder.AddItem(TAG_NAME, name);
			}
		}

		/* Get song duration */
		{
			const char *path[] = { "duration", nullptr };

			yajl_val duration = yajl_tree_get(entry, path, yajl_t_number);

			if(YAJL_IS_NUMBER(duration)) {
				tag_builder.SetDuration(
					SignedSongTime(std::chrono::seconds(YAJL_GET_INTEGER(duration)))
				);
			}
		}

		/* Get webpage url */
		{
			const char *path[] = { "webpage_url", nullptr };
			char *url = YAJL_GET_STRING(yajl_tree_get(entry, path, yajl_t_string));

			if(url) {
				songs.emplace_front(url, tag_builder.Commit());
			}
		}
	}

	songs.reverse();
	return std::make_unique<MemorySongEnumerator>(std::move(songs));
}

static const char *const playlist_youtube_schemes[] = {
	"https",
	nullptr
};

const PlaylistPlugin youtube_playlist_plugin =
	PlaylistPlugin("youtube", playlist_youtube_open)
	.WithInit(playlist_youtube_init)
	.WithSchemes(playlist_youtube_schemes);
