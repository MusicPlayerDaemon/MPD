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

#include "Id3Picture.hxx"
#include "Handler.hxx"
#include "util/ByteOrder.hxx"
#include "util/StringView.hxx"

#include <cstdint>
#include <string>

static StringView
ReadString(std::span<const std::byte> &src) noexcept
{
	if (src.size() < 4)
		return nullptr;

	const size_t length = *(const PackedBE32 *)(const void *)src.data();
	src = src.subspan(4);

	if (src.size() < length)
		return nullptr;

	const std::string_view result{(const char *)src.data(), length};
	src = src.subspan(length);
	return result;
}

void
ScanId3Apic(std::span<const std::byte> buffer, TagHandler &handler) noexcept
{
	if (buffer.size() < 4)
		return;

	buffer = buffer.subspan(4); /* picture type */

	const auto mime_type = ReadString(buffer);
	if (mime_type.IsNull())
		return;

	/* description */
	if (ReadString(buffer).IsNull())
		return;

	if (buffer.size() < 20)
		return;

	buffer = buffer.subspan(16);

	const size_t image_size = *(const PackedBE32 *)(const void *)buffer.data();
	buffer = buffer.subspan(4);

	if (buffer.size() < image_size)
		return;

	const auto image = buffer.first(image_size);

	// TODO: don't copy MIME type, pass StringView to TagHandler::OnPicture()
	handler.OnPicture(std::string(mime_type.data, mime_type.size).c_str(),
			  image);
}
