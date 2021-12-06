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

#include "config.h"
#include "tag/Id3Load.hxx"
#include "tag/Rva2.hxx"
#include "tag/ReplayGainInfo.hxx"
#include "thread/Mutex.hxx"
#include "fs/Path.hxx"
#include "input/InputStream.hxx"
#include "input/LocalOpen.hxx"
#include "util/PrintException.hxx"

#include <id3tag.h>

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#include <stdlib.h>
#include <stdio.h>

static void
DumpReplayGainTuple(const char *name, const ReplayGainTuple &tuple)
{
	if (tuple.IsDefined())
		fprintf(stderr, "replay_gain[%s]: gain=%f peak=%f\n",
			name, (double)tuple.gain, (double)tuple.peak);
}

static void
DumpReplayGainInfo(const ReplayGainInfo &info)
{
	DumpReplayGainTuple("album", info.album);
	DumpReplayGainTuple("track", info.track);
}

int main(int argc, char **argv)
try {
#ifdef HAVE_LOCALE_H
	/* initialize locale */
	setlocale(LC_CTYPE,"");
#endif

	if (argc != 2) {
		fprintf(stderr, "Usage: read_rva2 FILE\n");
		return EXIT_FAILURE;
	}

	const Path path = Path::FromFS(argv[1]);

	Mutex mutex;

	auto is = OpenLocalInputStream(path, mutex);

	const auto tag = tag_id3_load(*is);
	if (tag == nullptr) {
		fprintf(stderr, "No ID3 tag found\n");
		return EXIT_FAILURE;
	}

	ReplayGainInfo replay_gain;
	replay_gain.Clear();

	bool success = tag_rva2_parse(tag.get(), replay_gain);
	if (!success) {
		fprintf(stderr, "No RVA2 tag found\n");
		return EXIT_FAILURE;
	}

	DumpReplayGainInfo(replay_gain);

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
