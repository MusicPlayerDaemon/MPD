// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Id3MixRamp.hxx"
#include "MixRampParser.hxx"
#include "MixRampInfo.hxx"

#include <id3tag.h>

#include <stdlib.h>

MixRampInfo
Id3ToMixRampInfo(const struct id3_tag *tag) noexcept
{
	MixRampInfo result;

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

		ParseMixRampTag(result, key, value);

		free(key);
		free(value);
	}

	return result;
}
