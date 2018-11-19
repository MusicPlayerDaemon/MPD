/*
 * Copyright 2003-2018 The Music Player Daemon Project
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

#include "Handler.hxx"
#include "Builder.hxx"
#include "AudioFormat.hxx"
#include "util/ASCII.hxx"
#include "util/StringFormat.hxx"

#include <stdlib.h>

void
NullTagHandler::OnAudioFormat(gcc_unused AudioFormat af) noexcept
{
}

void
AddTagHandler::OnDuration(SongTime duration) noexcept
{
	tag.SetDuration(duration);
}

void
AddTagHandler::OnTag(TagType type, const char *value) noexcept
{
	if (type == TAG_TRACK || type == TAG_DISC) {
		/* filter out this extra data and leading zeroes */
		char *end;
		unsigned n = strtoul(value, &end, 10);
		if (value != end)
			tag.AddItem(type, StringFormat<21>("%u", n));
	} else
		tag.AddItem(type, value);
}

void
FullTagHandler::OnPair(const char *name, gcc_unused const char *value) noexcept
{
	if (StringEqualsCaseASCII(name, "cuesheet"))
		tag.SetHasPlaylist(true);
}

void
FullTagHandler::OnAudioFormat(AudioFormat af) noexcept
{
	if (audio_format != nullptr)
		*audio_format = af;
}
