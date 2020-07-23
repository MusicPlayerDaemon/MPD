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
#include "tag/Builder.hxx"
#include "tag/Tag.hxx"
#include "song/DetachedSong.hxx"
#include "util/Alloc.hxx"
#include "util/ScopeExit.hxx"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <string>
#include <forward_list>
#include <utility>

static bool playlist_youtube_init(const ConfigBlock &block)
{
	return WEXITSTATUS(system("youtube-dl --version > /dev/null")) == 0;
}

static std::unique_ptr<SongEnumerator>
playlist_youtube_open(const char *uri, Mutex &mutex)
{

	/* lazy way to prevent command injection */
	if(strchr(uri, '\'')) {
		return nullptr;
	}

	char *cmd = xstrcatdup("youtube-dl --flat-playlist --ignore-errors --get-id --get-title '", uri, "'");
	FILE *stream = popen(cmd, "r");
	free(cmd);

	if(!stream) return nullptr;

	std::forward_list<DetachedSong> songs;
	std::string video_url;

	char *line = nullptr;
	size_t line_size = 0;
	AtScopeExit(line) { free(line); };

	while(true) {
		/* Get song title */
		ssize_t read = getline(&line, &line_size, stream);
		if(read > 0 && line[read-1] == '\n') line[read-1] = '\0';

		TagBuilder tag_builder;
		tag_builder.AddItem(TAG_NAME, line);

		/* Construct url from id */
		read = getline(&line, &line_size, stream);
		if(read < 0) break;
		if(read > 0 && line[read-1] == '\n') line[read-1] = '\0';

		video_url = "watch?v=";
		video_url += line;

		songs.emplace_front(video_url, tag_builder.Commit());
	}

	if(WEXITSTATUS(pclose(stream)) != 0) {
		return nullptr;
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
