// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Id3ReplayGain.hxx"
#include "Rva2.hxx"
#include "ReplayGainParser.hxx"
#include "ReplayGainInfo.hxx"

#include <id3tag.h>

#include <stdlib.h>

bool
Id3ToReplayGainInfo(ReplayGainInfo &rgi, const struct id3_tag *tag) noexcept
{
	bool found = false;

	rgi.Clear();

	struct id3_frame *frame;
	for (unsigned i = 0; (frame = id3_tag_findframe(tag, "TXXX", i)); i++) {
		if (frame->nfields < 3)
			continue;

		const id3_ucs4_t *const key_ucs4 =
			id3_field_getstring(&frame->fields[1]);
		const id3_ucs4_t *const value_ucs4 =
			id3_field_getstring(&frame->fields[2]);
		if (key_ucs4 == nullptr || value_ucs4 == nullptr)
			/* this happens with frames which libid3tag
			   could not parse, e.g. encrypted frames */
			continue;

		char *const key = (char *)id3_ucs4_latin1duplicate(key_ucs4);
		char *const value = (char *)id3_ucs4_latin1duplicate(value_ucs4);

		if (ParseReplayGainTag(rgi, key, value))
			found = true;

		free(key);
		free(value);
	}

	return found ||
		/* fall back on RVA2 if no replaygain tags found */
		tag_rva2_parse(tag, rgi);
}
