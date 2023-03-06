// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Id3Picture.hxx"
#include "Handler.hxx"
#include "util/ByteOrder.hxx"

#include <cstdint>
#include <string>

static std::string_view
ReadString(std::span<const std::byte> &src) noexcept
{
	if (src.size() < 4)
		return {};

	const size_t length = *(const PackedBE32 *)(const void *)src.data();
	src = src.subspan(4);

	if (src.size() < length)
		return {};

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
	if (mime_type.data() == nullptr)
		return;

	/* description */
	if (ReadString(buffer).data() == nullptr)
		return;

	if (buffer.size() < 20)
		return;

	buffer = buffer.subspan(16);

	const size_t image_size = *(const PackedBE32 *)(const void *)buffer.data();
	buffer = buffer.subspan(4);

	if (buffer.size() < image_size)
		return;

	const auto image = buffer.first(image_size);

	// TODO: don't copy MIME type, pass std::string_view to TagHandler::OnPicture()
	handler.OnPicture(std::string{mime_type}.c_str(),
			  image);
}
