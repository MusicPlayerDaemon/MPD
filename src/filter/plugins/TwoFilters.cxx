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

#include "TwoFilters.hxx"
#include "pcm/AudioFormat.hxx"
#include "util/ConstBuffer.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringBuffer.hxx"

ConstBuffer<void>
TwoFilters::FilterPCM(ConstBuffer<void> src)
{
	return second->FilterPCM(first->FilterPCM(src));
}

ConstBuffer<void>
TwoFilters::Flush()
{
	auto result = first->Flush();
	if (!result.IsNull())
		/* Flush() output from the first Filter must be
		   filtered by the second Filter */
		return second->FilterPCM(result);

	return second->Flush();
}

std::unique_ptr<Filter>
PreparedTwoFilters::Open(AudioFormat &audio_format)
{
	auto a = first->Open(audio_format);

	const auto &a_out_format = a->GetOutAudioFormat();
	auto b_in_format = a_out_format;
	auto b = second->Open(b_in_format);

	if (b_in_format != a_out_format)
		throw FormatRuntimeError("Audio format not supported by filter '%s': %s",
					 second_name.c_str(),
					 ToString(a_out_format).c_str());

	return std::make_unique<TwoFilters>(std::move(a),
					    std::move(b));
}
