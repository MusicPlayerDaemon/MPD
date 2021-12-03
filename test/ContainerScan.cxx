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

#include "song/DetachedSong.hxx"
#include "SongSave.hxx"
#include "config/Data.hxx"
#include "decoder/DecoderList.hxx"
#include "decoder/DecoderPlugin.hxx"
#include "fs/Path.hxx"
#include "fs/NarrowPath.hxx"
#include "io/StdioOutputStream.hxx"
#include "io/BufferedOutputStream.hxx"
#include "util/PrintException.hxx"
#include "util/UriExtract.hxx"

#include <cassert>
#include <stdexcept>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

static const DecoderPlugin *
FindContainerDecoderPlugin(std::string_view suffix)
{
	return decoder_plugins_find([suffix](const DecoderPlugin &plugin){
			return plugin.container_scan != nullptr &&
				plugin.SupportsSuffix(suffix);
		});
}

static const DecoderPlugin *
FindContainerDecoderPlugin(Path path)
{
	const auto path_utf8 = path.ToUTF8Throw();
	const auto suffix = uri_get_suffix(path_utf8);
	if (suffix.empty())
		return nullptr;

	return FindContainerDecoderPlugin(suffix);
}

int main(int argc, char **argv)
try {
	if (argc != 2) {
		fprintf(stderr, "Usage: ContainerScan PATH\n");
		return EXIT_FAILURE;
	}

	const FromNarrowPath path = argv[1];

	const ScopeDecoderPluginsInit decoder_plugins_init({});

	const auto *plugin = FindContainerDecoderPlugin(path);
	if (plugin == nullptr) {
		fprintf(stderr, "No decoder found for this file\n");
		return EXIT_FAILURE;
	}

	const auto v = plugin->container_scan(path);
	if (v.empty()) {
		fprintf(stderr, "File is not a container\n");
		return EXIT_FAILURE;
	}

	StdioOutputStream sos(stdout);
	BufferedOutputStream bos(sos);

	for (const auto &song : v)
		song_save(bos, song);

	bos.Flush();

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
