/*
 * Copyright 2003-2016 The Music Player Daemon Project
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

#include "config.h"
#include "Log.hxx"
#include "decoder/DecoderList.hxx"
#include "decoder/DecoderPlugin.hxx"
#include "fs/Path.hxx"
#include "fs/io/StdioOutputStream.hxx"
#include "fs/io/BufferedOutputStream.hxx"
#include "util/UriUtil.hxx"

#include <stdexcept>

#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

static const DecoderPlugin *
FindContainerDecoderPlugin(const char *suffix)
{
	return decoder_plugins_find([suffix](const DecoderPlugin &plugin){
			return plugin.container_scan != nullptr &&
				plugin.SupportsSuffix(suffix);
		});
}

static const DecoderPlugin *
FindContainerDecoderPlugin(Path path)
{
	const auto utf8 = path.ToUTF8();
	if (utf8.empty())
		return nullptr;

	UriSuffixBuffer suffix_buffer;
	const char *const suffix = uri_get_suffix(utf8.c_str(), suffix_buffer);
	if (suffix == nullptr)
		return nullptr;

	return FindContainerDecoderPlugin(suffix);
}

int main(int argc, char **argv)
try {
	if (argc != 2) {
		fprintf(stderr, "Usage: ContainerScan PATH\n");
		return EXIT_FAILURE;
	}

	const Path path = Path::FromFS(argv[1]);

	decoder_plugin_init_all();

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
		bos.Format("%s\n", song.c_str());

	bos.Flush();

	decoder_plugin_deinit_all();

	return EXIT_SUCCESS;
} catch (const std::exception &e) {
	LogError(e);
	return EXIT_FAILURE;
}
