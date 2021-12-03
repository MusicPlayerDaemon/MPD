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

#include "TagSave.hxx"
#include "song/DetachedSong.hxx"
#include "playlist/SongEnumerator.hxx"
#include "input/InputStream.hxx"
#include "ConfigGlue.hxx"
#include "decoder/DecoderList.hxx"
#include "input/Init.hxx"
#include "event/Thread.hxx"
#include "playlist/PlaylistRegistry.hxx"
#include "playlist/PlaylistPlugin.hxx"
#include "fs/Path.hxx"
#include "fs/NarrowPath.hxx"
#include "io/BufferedOutputStream.hxx"
#include "io/StdioOutputStream.hxx"
#include "thread/Cond.hxx"
#include "util/PrintException.hxx"

#include <unistd.h>
#include <stdlib.h>

static void
tag_save(FILE *file, const Tag &tag)
{
	StdioOutputStream sos(file);
	WithBufferedOutputStream(sos, [&](auto &bos){
		tag_save(bos, tag);
	});
}

int main(int argc, char **argv)
try {
	const char *uri;

	if (argc != 3) {
		fprintf(stderr, "Usage: dump_playlist CONFIG URI\n");
		return EXIT_FAILURE;
	}

	const FromNarrowPath config_path = argv[1];
	uri = argv[2];

	/* initialize MPD */

	const auto config = AutoLoadConfigFile(config_path);

	EventThread io_thread;
	io_thread.Start();

	const ScopeInputPluginsInit input_plugins_init(config, io_thread.GetEventLoop());
	const ScopePlaylistPluginsInit playlist_plugins_init(config);
	const ScopeDecoderPluginsInit decoder_plugins_init(config);

	/* open the playlist */

	Mutex mutex;

	InputStreamPtr is;
	auto playlist = playlist_list_open_uri(uri, mutex);
	if (playlist == nullptr) {
		/* open the stream and wait until it becomes ready */

		is = InputStream::OpenReady(uri, mutex);

		/* open the playlist */

		playlist = playlist_list_open_stream(std::move(is), uri);
		if (playlist == nullptr) {
			fprintf(stderr, "Failed to open playlist\n");
			return 2;
		}
	}

	/* dump the playlist */

	std::unique_ptr<DetachedSong> song;
	while ((song = playlist->NextSong()) != nullptr) {
		printf("%s\n", song->GetURI());

		const unsigned start_ms = song->GetStartTime().ToMS();
		const unsigned end_ms = song->GetEndTime().ToMS();

		if (end_ms > 0)
			printf("range: %u:%02u..%u:%02u\n",
			       start_ms / 60000,
			       (start_ms / 1000) % 60,
			       end_ms / 60000,
			       (end_ms / 1000) % 60);
		else if (start_ms > 0)
			printf("range: %u:%02u..\n",
			       start_ms / 60000,
			       (start_ms / 1000) % 60);

		tag_save(stdout, song->GetTag());
	}

	/* deinitialize everything */

	playlist.reset();
	is.reset();

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
