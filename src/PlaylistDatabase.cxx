/*
 * Copyright 2003-2022 The Music Player Daemon Project
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

#include "PlaylistDatabase.hxx"
#include "db/PlaylistVector.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "io/LineReader.hxx"
#include "io/BufferedOutputStream.hxx"
#include "time/ChronoUtil.hxx"
#include "util/StringStrip.hxx"

#include <fmt/format.h>

#include <cstring>

#include <stdlib.h>

void
playlist_vector_save(BufferedOutputStream &os, const PlaylistVector &pv)
{
	for (const PlaylistInfo &pi : pv) {
		os.Fmt(FMT_STRING(PLAYLIST_META_BEGIN "{}\n"), pi.name);
		if (!IsNegative(pi.mtime))
			os.Fmt(FMT_STRING("mtime: {}\n"),
			       std::chrono::system_clock::to_time_t(pi.mtime));
		os.Write("playlist_end\n");
	}
}

void
playlist_metadata_load(LineReader &file, PlaylistVector &pv, const char *name)
{
	PlaylistInfo pm(name);

	char *line, *colon;
	const char *value;

	while ((line = file.ReadLine()) != nullptr &&
	       std::strcmp(line, "playlist_end") != 0) {
		colon = std::strchr(line, ':');
		if (colon == nullptr || colon == line)
			throw FmtRuntimeError("unknown line in db: {}", line);

		*colon++ = 0;
		value = StripLeft(colon);

		if (std::strcmp(line, "mtime") == 0)
			pm.mtime = std::chrono::system_clock::from_time_t(strtol(value, nullptr, 10));
		else
			throw FmtRuntimeError("unknown line in db: {}", line);
	}

	pv.UpdateOrInsert(std::move(pm));
}
