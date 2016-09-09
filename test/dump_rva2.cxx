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
#include "tag/Id3Load.hxx"
#include "tag/TagRva2.hxx"
#include "ReplayGainInfo.hxx"
#include "config/ConfigGlobal.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "fs/Path.hxx"
#include "input/InputStream.hxx"
#include "input/LocalOpen.hxx"
#include "Log.hxx"

#include <id3tag.h>

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#include <stdlib.h>
#include <stdio.h>

const char *
config_get_string(gcc_unused enum ConfigOption option,
		  const char *default_value)
{
	return default_value;
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
	Cond cond;

	auto is = OpenLocalInputStream(path, mutex, cond);

	const auto tag = tag_id3_load(*is);
	if (tag == NULL) {
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

	const ReplayGainTuple *tuple = &replay_gain.tuples[REPLAY_GAIN_ALBUM];
	if (tuple->IsDefined())
		fprintf(stderr, "replay_gain[album]: gain=%f peak=%f\n",
			tuple->gain, tuple->peak);

	tuple = &replay_gain.tuples[REPLAY_GAIN_TRACK];
	if (tuple->IsDefined())
		fprintf(stderr, "replay_gain[track]: gain=%f peak=%f\n",
			tuple->gain, tuple->peak);

	return EXIT_SUCCESS;
} catch (const std::exception &e) {
	LogError(e);
	return EXIT_FAILURE;
}
